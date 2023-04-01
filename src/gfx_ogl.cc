/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#include <iostream>

#include "gfx_ogl.h"
#include "logging.h"

#define GLAD_GL_IMPLEMENTATION
#include "gl.hpp"

namespace gfx {
  const vr::ETextureType TextureType = vr::TextureType_OpenGL;
}

namespace ogl {
  device::device() :
   zloop_() {
    // TODO: This would need to be moved if we ever have > 1 devices
    if (!glfwInit())
    {
      std::cout << "Failed to initialize OpenGL context" << std::endl;
      ovrly::logger::error("OGL error initializing glfw");
      return;
    }

    // This is a ghost window to own the GL context
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
    window_ = glfwCreateWindow(800, 600, "Ovrly opengl ghost", nullptr, nullptr);

    // Make default immediate context
    immediate_ = create_context();
  }

  tex2_ptr device::create_texture(int width, int height) {
    return nullptr;
  }

  Context_ptr device::create_context() {
    return std::make_shared<Context>(window_);
  }

  Context_ptr device::immediate_context() {
    return immediate_;
  }

  Context::Context(GLFWwindow* window) :
   context_() {
    glfwMakeContextCurrent(window);
    int version = gladLoadGLContext(&context_, glfwGetProcAddress);
    if (version == 0)
    {
        ovrly::logger::error("OGL error initializing openGL context");
        return;
    }

    ovrly::logger::info("Loaded OpenGL {}.{}", GLAD_VERSION_MAJOR(version), GLAD_VERSION_MINOR(version));
  }

  device_ptr create_device()
  {
    return std::make_shared<device>();
  }

  tex2::tex2(uint tex) {
  }

  void tex2::bind(Context_ptr const& ctx) {
  }

  void tex2::unbind() {
  }

  uint32_t tex2::width() const {
    return 0;
  }

  uint32_t tex2::height() const {
    return 0;
  }

  void tex2::copy_from(const void* buffer, uint32_t stride, uint32_t rows) {
  }

  void *tex2::ovr_handle() const {
    return nullptr;
  }
}

