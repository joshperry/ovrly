/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#pragma once

#include <memory>
#include <string>
#include <thread>

#include <glad/gl.hpp>
#include <GLFW/glfw3.h>

#include "openvr.h"
#include "gfx_shared.h"

namespace ogl {
  class tex2;
  typedef std::shared_ptr<tex2> tex2_ptr;
  class device;
  typedef std::shared_ptr<device> device_ptr;
	device_ptr create_device();
  template<class T> class ScopedBinder;
  class Context;
  typedef std::shared_ptr<Context> Context_ptr;
}

namespace gfx {
  using ogl::tex2;
  using ogl::tex2_ptr;

  using ogl::device;
  using ogl::device_ptr;
  using ogl::create_device;

  template<class T> using ScopedBinder = ogl::ScopedBinder<T>;

  extern const vr::ETextureType TextureType;
}

namespace ogl {
  // a lot of these machinations are to match d3d's more granular API :/
  // this could be factored down further, but splitting into multiple contexts
  // (even when using only one) will probably be shortest MVP

  // Encapsulates a glfw context, we only ever have one per window
	class Context
	{
	public:
		Context(GLFWwindow*);

		void flush();

  private:
    GladGLContext context_;
	};

  class device {
  public:
    device();
		tex2_ptr create_texture(int width, int height);
    Context_ptr create_context();
    Context_ptr immediate_context();

  private:
    // Hidden GL context window message pump
    std::unique_ptr<std::thread> zloop_;

    // The ghost window
    GLFWwindow* window_;

    // Single-thread usage "immediate mode" context
    Context_ptr immediate_;
  };

	class tex2
	{
	public:
    tex2(uint tex);

    // Used to un/bind the texture to a context to get it filled
		void bind(Context_ptr const& ctx);
		void unbind();

    // Size in texels of the texture that the browser is renderd into
		uint32_t width() const;
		uint32_t height() const;

    // A handle in the format expected by openvr overlay texture
    void* ovr_handle() const;

    // Used for copying the CEF client area into a texture
		void copy_from(const void* buffer, uint32_t stride, uint32_t rows);
	};

	template<class T>
	class ScopedBinder
	{
	public:
		ScopedBinder(
			std::shared_ptr<device> const& device,
			std::shared_ptr<T> const& target)
			: target_(target)
		{
			if (target_) { target_->bind(device->immediate_context()); }
		}
		~ScopedBinder() { if (target_) { target_->unbind(); } }
	private:
		std::shared_ptr<T> const target_;
	};
}
