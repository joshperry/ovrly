/*
 * Thanks to Greg Wessels and daktronics for this code from https://github.com/daktronics/cef-mixer/
 * Licensed under the MIT License
 */

#pragma once

#include <d3d11_1.h>
#include <memory>
#include <string>

#include "openvr.h"
#include "gfx_shared.h"

namespace d3d {
	class SwapChain;
	class Geometry;
	class Effect;
	class Texture2D;
	class Context;
  template<class T> class ScopedBinder;
}

namespace gfx {
  enum class BufferFormat {
    RGBA = DXGI_FORMAT_R8G8B8A8_UNORM,
    BGRA = DXGI_FORMAT_B8G8R8A8_UNORM,
  };

  using d3d::Texture2D tex2;
  typedef std::shared_ptr<tex2> tex2_ptr;

  using d3d::Device device;
  typedef std::shared_ptr<device> device_ptr;
  using d3d::create_device;

  template<class T> using ScopedBinder = d3d::ScopedBinder<T>;

  extern const vr::ETextureType TextureType = vr::TextureType_DirectX;
}

namespace d3d {
	class Context
	{
	public:
		Context(ID3D11DeviceContext*);

		void flush();

		operator ID3D11DeviceContext*() {
			return ctx_.get();
		}

	private:

		std::shared_ptr<ID3D11DeviceContext> const ctx_;
	};

	//
	// encapsulate a D3D11 Device object
	//
	class Device
	{
	public:
		Device(ID3D11Device*, ID3D11DeviceContext*);

		std::string adapter_name() const;

		operator ID3D11Device*() {
			return device_.get();
		}

		std::shared_ptr<Context> immediate_context();

		std::shared_ptr<SwapChain> create_swapchain(HWND, int width=0, int height=0);

		std::shared_ptr<Geometry> create_quad(
					float x, float y, float width, float height, bool flip=false);

		std::shared_ptr<Texture2D> create_texture(
					int width,
					int height,
					gfx::BufferFormat format,
					size_t row_stride);

		std::shared_ptr<Texture2D> open_shared_texture(void*);

		std::shared_ptr<Effect> create_default_effect();

		std::shared_ptr<Effect> create_effect(
						std::string const& vertex_code,
						std::string const& vertex_entry,
						std::string const& vertex_model,
						std::string const& pixel_code,
						std::string const& pixel_entry,
						std::string const& pixel_model);

	private:

		std::shared_ptr<ID3DBlob> compile_shader(
					std::string const& source_code,
					std::string const& entry_point,
					std::string const& model);

		HMODULE _lib_compiler;

		std::shared_ptr<ID3D11Device> const device_;
		std::shared_ptr<Context> const ctx_;
	};

	template<class T>
	class ScopedBinder
	{
	public:
		ScopedBinder(
			std::shared_ptr<Device> const& device,
			std::shared_ptr<T> const& target)
			: target_(target)
		{
			if (target_) { target_->bind(device->immediate_context()); }
		}
		~ScopedBinder() { if (target_) { target_->unbind(); } }
	private:
		std::shared_ptr<T> const target_;
	};

	//
	// encapsulate a DXGI swapchain for a window
	//
	class SwapChain
	{
	public:
		SwapChain(
				IDXGISwapChain*,
				ID3D11RenderTargetView*,
				ID3D11SamplerState*,
				ID3D11BlendState*);

		void bind(std::shared_ptr<Context> const& ctx);
		void unbind();

		void clear(float red, float green, float blue, float alpha);

		void present(int sync_interval);
		void resize(int width, int height);

	private:

		std::shared_ptr<ID3D11SamplerState> const sampler_;
		std::shared_ptr<ID3D11BlendState> const blender_;
		std::shared_ptr<IDXGISwapChain> const swapchain_;
		std::shared_ptr<ID3D11RenderTargetView> rtv_;
		std::shared_ptr<Context> ctx_;
	};

	class Texture2D
	{
	public:
		Texture2D(
			ID3D11Texture2D* tex,
			ID3D11ShaderResourceView* srv);

		void bind(std::shared_ptr<Context> const& ctx);
		void unbind();

		uint32_t width() const;
		uint32_t height() const;
    gfx::BufferFormat format() const;

		bool has_mutex() const;

		bool lock_key(uint64_t key, uint32_t timeout_ms);
		void unlock_key(uint64_t key);

    void* ovr_handle() const;
		void* share_handle() const;

		void copy_from(std::shared_ptr<Texture2D> const&);

		void copy_from(const void* buffer, uint32_t stride, uint32_t rows);

    std::shared_ptr<ID3D11Texture2D> const texture_;

	private:

		HANDLE share_handle_;

		std::shared_ptr<ID3D11ShaderResourceView> const srv_;
		std::shared_ptr<IDXGIKeyedMutex> keyed_mutex_;
		std::shared_ptr<Context> ctx_;
	};

	class Effect
	{
	public:
		Effect(
				ID3D11VertexShader* vsh,
				ID3D11PixelShader* psh,
				ID3D11InputLayout* layout);

		void bind(std::shared_ptr<Context> const& ctx);
		void unbind();

	private:

		std::shared_ptr<ID3D11VertexShader> const vsh_;
		std::shared_ptr<ID3D11PixelShader> const psh_;
		std::shared_ptr<ID3D11InputLayout> const layout_;
		std::shared_ptr<Context> ctx_;
	};


	class Geometry
	{
	public:
		Geometry(
			D3D_PRIMITIVE_TOPOLOGY primitive,
			uint32_t vertices,
			uint32_t stride,
			ID3D11Buffer*);

		void bind(std::shared_ptr<Context> const& ctx);
		void unbind();

		void draw();

	private:

		D3D_PRIMITIVE_TOPOLOGY primitive_;
		uint32_t vertices_;
		uint32_t stride_;
		std::shared_ptr<ID3D11Buffer> const buffer_;
		std::shared_ptr<Context> ctx_;
	};


	std::shared_ptr<Device> create_device();
}
