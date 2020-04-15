/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#include "webovrly.h"

#include <functional>
#include <memory>
#include <d3d11_1.h>
#include <d3dcompiler.h>
#include <directxmath.h>
#include <openvr.h>


namespace ovrly{ namespace web{

  // Module local
  namespace {

  #define	D3D11_SDK_VERSION	( 7 )

  template<class T>
  std::shared_ptr<T> to_com_ptr(T* obj)
  {
    return std::shared_ptr<T>(obj, [](T* p) { if (p) p->Release(); });
  }

  class Context {
  public:
    Context(ID3D11DeviceContext* ctx)
      : ctx_(to_com_ptr(ctx))
    {
    }

    void flush() {
      ctx_->Flush();
    }

    operator ID3D11DeviceContext* () {
      return ctx_.get();
    }

  private:

    std::shared_ptr<ID3D11DeviceContext> const ctx_;
  };

  template<class T>
  class ScopedBinder
  {
  public:
    ScopedBinder(
      std::shared_ptr<Context> const& ctx,
      std::shared_ptr<T> const& target)
      : target_(target)
    {
      if (target_) { target_->bind(ctx); }
    }
    ~ScopedBinder() { if (target_) { target_->unbind(); } }
  private:
    std::shared_ptr<T> const target_;
  };

  class Texture2D {
    public:
      Texture2D(
        ID3D11Texture2D* tex,
        ID3D11ShaderResourceView* srv)
        : texture_(to_com_ptr(tex))
        , srv_(to_com_ptr(srv))
      {
        share_handle_ = nullptr;

        IDXGIResource* res = nullptr;
        if (SUCCEEDED(texture_->QueryInterface(
          __uuidof(IDXGIResource), reinterpret_cast<void**>(&res))))
        {
          res->GetSharedHandle(&share_handle_);
          res->Release();
        }

        // are we using a keyed mutex?
        IDXGIKeyedMutex* mutex = nullptr;
        if (SUCCEEDED(texture_->QueryInterface(__uuidof(IDXGIKeyedMutex), (void**)&mutex))) {
          keyed_mutex_ = to_com_ptr(mutex);
        }
      }

      uint32_t width() const
      {
        D3D11_TEXTURE2D_DESC desc;
        texture_->GetDesc(&desc);
        return desc.Width;
      }

      uint32_t height() const
      {
        D3D11_TEXTURE2D_DESC desc;
        texture_->GetDesc(&desc);
        return desc.Height;
      }

      DXGI_FORMAT format() const
      {
        D3D11_TEXTURE2D_DESC desc;
        texture_->GetDesc(&desc);
        return desc.Format;
      }

      bool has_mutex() const
      {
        return (keyed_mutex_.get() != nullptr);
      }

      bool lock_key(uint64_t key, uint32_t timeout_ms)
      {
        if (keyed_mutex_) {
          auto const hr = keyed_mutex_->AcquireSync(key, timeout_ms);
          return SUCCEEDED(hr);
        }
        return true;
      }

      void unlock_key(uint64_t key)
      {
        if (keyed_mutex_) {
          keyed_mutex_->ReleaseSync(key);
        }
      }

      void bind(std::shared_ptr<Context> const& ctx)
      {
        ctx_ = ctx;
        ID3D11DeviceContext* d3d11_ctx = (ID3D11DeviceContext*)(*ctx_);
        if (srv_)
        {
          ID3D11ShaderResourceView* views[1] = { srv_.get() };
          d3d11_ctx->PSSetShaderResources(0, 1, views);
        }
      }

      void unbind()
      {
      }

      void* share_handle() const
      {
        return share_handle_;
      }

      void copy_from(std::shared_ptr<Texture2D> const& other)
      {
        ID3D11DeviceContext* d3d11_ctx = (ID3D11DeviceContext*)(*ctx_);
        assert(d3d11_ctx);
        if (other) {
          d3d11_ctx->CopyResource(texture_.get(), other->texture_.get());
        }
      }

      void copy_from(const void* buffer, uint32_t stride, uint32_t rows)
      {
        if (!buffer) {
          return;
        }

        ID3D11DeviceContext* d3d11_ctx = (ID3D11DeviceContext*)(*ctx_);
        assert(d3d11_ctx);

        D3D11_MAPPED_SUBRESOURCE res;
        auto const hr = d3d11_ctx->Map(texture_.get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &res);
        if (SUCCEEDED(hr))
        {
          if (rows == height())
          {
            if (res.RowPitch == stride) {
              memcpy(res.pData, buffer, stride * rows);
            }
            else {
              const uint8_t* src = (const uint8_t*)buffer;
              uint8_t* dst = (uint8_t*)res.pData;
              uint32_t cb = res.RowPitch < stride ? res.RowPitch : stride;
              for (uint32_t y = 0; y < rows; ++y)
              {
                memcpy(dst, src, cb);
                src += stride;
                dst += res.RowPitch;
              }
            }
          }

          d3d11_ctx->Unmap(texture_.get(), 0);
        }
      }

      std::shared_ptr<ID3D11Texture2D> const texture_;

    private:

      HANDLE share_handle_;

      std::shared_ptr<ID3D11ShaderResourceView> const srv_;
      std::shared_ptr<IDXGIKeyedMutex> keyed_mutex_;
      std::shared_ptr<Context> ctx_;
  };

  class Device {
    public:
      Device(ID3D11Device* pdev, ID3D11DeviceContext* pctx)
        : device_(to_com_ptr(pdev))
        , ctx_(std::make_shared<Context>(pctx))
      {
        //_lib_compiler = LoadLibrary(L"d3dcompiler_47.dll");
      }

      std::wstring Device::adapter_name() const
      {
        IDXGIDevice* dxgi_dev = nullptr;
        auto hr = device_->QueryInterface(__uuidof(dxgi_dev), (void**)&dxgi_dev);
        if (SUCCEEDED(hr))
        {
          IDXGIAdapter* dxgi_adapt = nullptr;
          hr = dxgi_dev->GetAdapter(&dxgi_adapt);
          dxgi_dev->Release();
          if (SUCCEEDED(hr))
          {
            DXGI_ADAPTER_DESC desc;
            hr = dxgi_adapt->GetDesc(&desc);
            dxgi_adapt->Release();
            if (SUCCEEDED(hr)) {
              return desc.Description;
            }
          }
        }

        return L"n/a";
      }

      std::shared_ptr<Texture2D> Device::create_texture(
        int width,
        int height,
        DXGI_FORMAT format,
        const void* data,
        size_t row_stride)
      {
        D3D11_TEXTURE2D_DESC td;
        td.ArraySize = 1;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        td.CPUAccessFlags = data ? 0 : D3D11_CPU_ACCESS_WRITE;
        td.Format = format;
        td.Width = width;
        td.Height = height;
        td.MipLevels = 1;
        td.MiscFlags = 0;
        td.SampleDesc.Count = 1;
        td.SampleDesc.Quality = 0;
        td.Usage = data ? D3D11_USAGE_DEFAULT : D3D11_USAGE_DYNAMIC;

        D3D11_SUBRESOURCE_DATA srd;
        srd.pSysMem = data;
        srd.SysMemPitch = static_cast<uint32_t>(row_stride);
        srd.SysMemSlicePitch = 0;

        ID3D11Texture2D* tex = nullptr;
        auto hr = device_->CreateTexture2D(&td, data ? &srd : nullptr, &tex);
        if (FAILED(hr)) {
          return nullptr;
        }

        D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc;
        srv_desc.Format = td.Format;
        srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        srv_desc.Texture2D.MostDetailedMip = 0;
        srv_desc.Texture2D.MipLevels = 1;

        ID3D11ShaderResourceView* srv = nullptr;
        hr = device_->CreateShaderResourceView(tex, &srv_desc, &srv);
        if (FAILED(hr))
        {
          tex->Release();
          return nullptr;
        }

        return std::make_shared<Texture2D>(tex, srv);
      }

      std::shared_ptr<Context> context() {
        return ctx_;
      }

    private:

      std::shared_ptr<ID3D11Device> const device_;
      std::shared_ptr<Context> const ctx_;
  };

  std::shared_ptr<Device> create_device()
  {
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_FEATURE_LEVEL feature_levels[] =
    {
      D3D_FEATURE_LEVEL_11_1,
      D3D_FEATURE_LEVEL_11_0,
      D3D_FEATURE_LEVEL_10_1,
      D3D_FEATURE_LEVEL_10_0,
      //D3D_FEATURE_LEVEL_9_3,
    };
    UINT num_feature_levels = sizeof(feature_levels) / sizeof(feature_levels[0]);


    ID3D11Device* pdev = nullptr;
    ID3D11DeviceContext* pctx = nullptr;

    D3D_FEATURE_LEVEL selected_level;
    HRESULT hr = D3D11CreateDevice(
      nullptr,
      D3D_DRIVER_TYPE_HARDWARE,
      nullptr,
      flags, feature_levels,
      num_feature_levels,
      D3D11_SDK_VERSION,
      &pdev,
      &selected_level,
      &pctx);

    if (hr == E_INVALIDARG)
    {
      // DirectX 11.0 platforms will not recognize D3D_FEATURE_LEVEL_11_1
      // so we need to retry without it
      hr = D3D11CreateDevice(
        nullptr,
        D3D_DRIVER_TYPE_HARDWARE,
        nullptr,
        flags,
        &feature_levels[1],
        num_feature_levels - 1,
        D3D11_SDK_VERSION,
        &pdev,
        &selected_level,
        &pctx);
    }

    if (SUCCEEDED(hr))
    {
      auto const dev = std::make_shared<Device>(pdev, pctx);

      std::wstringstream ss;
      ss << L"d3d11: selected adapter: " << dev->adapter_name() << std::endl;
      ss << L"d3d11: selected feature level :" << selected_level << std::endl;
      OutputDebugString(ss.str().c_str());

      return dev;
    }

    return nullptr;
  }

    class ClientHandler: public Client,
                          public CefClient,
                          public CefLifeSpanHandler,
                          public CefRequestHandler,
                          public CefRenderHandler
    {
      public:
        ClientHandler() {
          device_ = create_device();
          auto ovrly = vr::VROverlay();
          if(ovrly) {
            // First see if the overlay exists already
            auto err = ovrly->FindOverlay("ovrlymain", &vroverlay_);
            if (err != vr::VROverlayError_None) {
              err = ovrly->CreateOverlay("ovrlymain", "Main ovrly", &vroverlay_);
              if (err != vr::VROverlayError_None) {
                std::wstringstream ss;
                ss << L"OVRLY VR error creating overlay" << err;
                OutputDebugString(ss.str().c_str());
              }
            }

            if(vroverlay_ != vr::k_ulOverlayHandleInvalid) {
              ovrly->SetOverlayWidthInMeters(vroverlay_, 0.5f);
              ovrly->ShowOverlay(vroverlay_);
            }
          } else {
            OutputDebugString(L"OVRLY VR overlay interface unavailable");
          }
        }

        void OnBeforeClose(CefRefPtr<CefBrowser> browser) override
        {
          SubOnBeforeClose(browser);
        }

        void OnRenderProcessTerminated(CefRefPtr< CefBrowser > browser, CefRequestHandler::TerminationStatus status) override
        {
          SubOnRenderProcessTerminated(browser, status);
        }

        bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
          CefRefPtr<CefFrame> frame,
          CefRefPtr<CefRequest> request,
          bool user_gesture,
          bool is_redirect) override
        {
          return SubOnBeforeBrowse(browser, frame, request, user_gesture, is_redirect);
        }

        bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
          CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override
        {
          return SubOnProcessMessageReceived(browser, frame, source_process, message);
        }

        void GetViewRect(CefRefPtr< CefBrowser > browser, CefRect& rect) override {
          OutputDebugString(L"OVRLY GetViewRect");
          rect.Set(0, 0, 800, 600);
        }

        void OnPaint(CefRefPtr<CefBrowser> browser, CefRenderHandler::PaintElementType type,
            const CefRenderHandler::RectList& dirtyRects, const void* buffer, int width, int height ) override
        {
          if(vroverlay_ == vr::k_ulOverlayHandleInvalid) {
            return;
          }

          OutputDebugString(L"OVRLY OnPainted");

          uint32_t stride = width * 4;

          if (!texture_ ||
            (texture_->width() != static_cast<UINT>(width)) ||
            (texture_->height() != static_cast<UINT>(height)))
          {
            texture_ = device_->create_texture(width, height, DXGI_FORMAT_B8G8R8A8_UNORM, nullptr, 0);

            ScopedBinder<Texture2D> binder(device_->context(), texture_);
            texture_->copy_from(
              buffer,
              stride,
              texture_->height());

            vrtexture_ = new vr::Texture_t{
              texture_->texture_.get(),
              vr::TextureType_DirectX,
              vr::ColorSpace_Gamma
            };
            vr::VROverlay()->SetOverlayTexture(vroverlay_, vrtexture_);
          } else {
            ScopedBinder<Texture2D> binder(device_->context(), texture_);
            texture_->copy_from(
              buffer,
              stride,
              texture_->height());

            vrtexture_ = new vr::Texture_t{
              texture_->texture_.get(),
              vr::TextureType_DirectX,
              vr::ColorSpace_Gamma
            };
            vr::VROverlay()->SetOverlayTexture(vroverlay_, vrtexture_);
          }

          //vr::VROverlay()->SetOverlayRaw(vroverlay_, const_cast<void*>(buffer), width, height, 4);
        }

      private:

        std::shared_ptr<Device> device_;
        std::shared_ptr<Texture2D> texture_;
        vr::Texture_t* vrtexture_;
        vr::VROverlayHandle_t vroverlay_;

        IMPLEMENT_REFCOUNTING(ClientHandler);
    };

    class WebClient : public CefClient {
      public:
        WebClient(CefRefPtr<ClientHandler> handler) : handler_(handler) {

        }

        CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
          return handler_;
        }

        CefRefPtr<CefRequestHandler> GetRequestHandler() override {
          return handler_;
        }

        CefRefPtr<CefRenderHandler> GetRenderHandler() override {
          return handler_;
        }

        bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
          CefRefPtr<CefFrame> frame,
          CefProcessId source_process,
          CefRefPtr<CefProcessMessage> message) override
        {
          return handler_->OnProcessMessageReceived(browser, frame, source_process, message);
        }

      private:
        CefRefPtr<ClientHandler> handler_;

        IMPLEMENT_REFCOUNTING(WebClient);
    };
  } // module local


 /*
 * Module exports
 */

Event<Client&> OnClient;

CefRefPtr<CefClient> Create() {
  CefRefPtr<ClientHandler> handler = new ClientHandler();

  OnClient(*handler);

  auto client = new WebClient(handler);

  // We want o create a windowless browser
  CefWindowInfo window_info;
  window_info.windowless_rendering_enabled = true;
  window_info.SetAsWindowless(nullptr);

  // Run the rendering at 15fps for now
  CefBrowserSettings settings;
  settings.windowless_frame_rate = 15;


  OutputDebugString(L"OVRLY Creating Web Overlay");

  // Rez the actual chromium browser
  CefBrowserHost::CreateBrowser(window_info, client, "https://dumb.com", settings, nullptr, nullptr);

  return client;
}

}} // module exports
