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
#include <openvr.h>

#include "logging.h"

namespace ovr = ::vr;
using namespace std::placeholders;

namespace ovrly{ namespace web{

// Module local
namespace {

  class ClientHandler: public Client,
                        public CefClient,
                        public CefLifeSpanHandler,
                        public CefRequestHandler,
                        public CefRenderHandler
  {
    public:
      ClientHandler() { }

      void OnBeforeClose(CefRefPtr<CefBrowser> browser) override
      {
        SubOnBeforeClose(browser);
      }

      void OnAfterCreated(CefRefPtr<CefBrowser> browser) {
        browser_ = browser;
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
        logger::debug("(web) GetViewRect");
        rect.Set(0, 0, size_.x, size_.y);
      }

      void SetSize(mathfu::vec2i size) {
        size_ = size;

        // Notify the browser of the resize if it exists yet
        if (browser_)
          browser_->GetHost()->WasResized();
      }

      void SetPaintCallback(std::function<void(CefRenderHandler::RectList, const void*)> callback) {
        paintcb_ = callback;
      }

      void OnPaint(CefRefPtr<CefBrowser> browser, CefRenderHandler::PaintElementType type,
          const CefRenderHandler::RectList& dirtyRects, const void* buffer, int width, int height ) override
      {
        // Call the paint callback
        paintcb_(dirtyRects, buffer);
      }

    private:

      CefRefPtr<CefBrowser> browser_;
      mathfu::vec2i size_;
      std::function<void(CefRenderHandler::RectList, const void*)> paintcb_;

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

      CefRefPtr<ClientHandler> GetHandler() {
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

  class WebOverlay : public vr::Overlay {
    public:
      WebOverlay(const std::string &name, mathfu::vec2 size, CefRefPtr<WebClient> client) :
        vr::Overlay(name, size),
        client_(client)
      {
        client->GetHandler()->SetPaintCallback([this](auto rects, auto buffer) {
          cefpaint(rects, buffer);
        });

        // Set the initial size
        onLayout(size);
      }

    protected:
      void onLayout(mathfu::vec2 size) override {
        // Calculate the pixel dimensions that the overlay will be rendered at
        // TODO: Calculate this based on optimum pixels/cm display in VR
        mathfu::vec2i psize(800 * size.y, 800);

        // Tell the overlay what pixel dimensions the browser will render to
        // opengl is zero bottom right
        updateTargetSize(psize, {{0, 1}, {1, 0}});

        // Tell the browser the new pixel size
        client_->GetHandler()->SetSize(psize);
      }

    private:
      /**
        * This gets registered with the handler to be called when the offscreen
        * browser provides a frame to paint.
        */
      void cefpaint(const CefRenderHandler::RectList &dirtyRects, const void *buffer) {
        // Convert from cef rect list to mathfu rects
        std::vector<mathfu::recti> rects;
        rects.resize(dirtyRects.size());
        std::transform(dirtyRects.begin(), dirtyRects.end(), rects.begin(), [](const CefRect &rect) {
          return mathfu::recti(rect.x, rect.y, rect.width, rect.height);
        });

        // Tell the overlay base class to render the frame
        render(buffer, rects);
      }

      CefRefPtr<WebClient> client_;
  };

 } // module local


 /*
 * Module exports
 */

Event<Client&> OnClient;

std::unique_ptr<vr::Overlay> Create(const std::string &name, mathfu::vec2 size, std::string const &url) {
  CefRefPtr<ClientHandler> handler = new ClientHandler();

  OnClient(*handler);

  CefRefPtr<WebClient> client = new WebClient(handler);

  // Create a windowless browser for off-screen rendering
  CefWindowInfo window_info;
  window_info.windowless_rendering_enabled = true;
  window_info.SetAsWindowless(0);

  // Specify the chromium render framerate
  CefBrowserSettings settings;
  settings.windowless_frame_rate = 30;

  logger::info("OVRLY Creating Web Overlay");

  // Rez the actual chromium browser
  CefBrowserHost::CreateBrowser(window_info, client, url.c_str(), settings, nullptr, nullptr);

  return std::make_unique<WebOverlay>(name, size, client);
}

}} // module exports
