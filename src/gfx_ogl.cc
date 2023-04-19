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
    if (!glfwInit()) {
      std::cout << "Failed to initialize OpenGL context" << std::endl;
      ovrly::logger::error("OGL error initializing glfw");
      return;
    }

    // This is a ghost window to own the GL context
    glfwWindowHint(GLFW_VISIBLE, GL_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window_ = glfwCreateWindow(800, 600, "Ovrly opengl ghost", nullptr, nullptr);

    // Make default immediate context
    immediate_ = create_context();
  }

  tex2_ptr device::create_texture(int width, int height) {
    return std::make_shared<tex2>(this, width, height);
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

  tex2::tex2(device *device, int width, int height)
  : device_(device), width_(width), height_(height) {
    auto gl = device_->immediate_context()->gl();
    gl->GenTextures(1, &texture_);
    gl->BindTexture(GL_TEXTURE_2D, texture_);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    gl->TexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Test contents
    const unsigned char data[] = {
      255, 0, 0, 255,
      0, 255, 0, 255,
      0, 0, 255, 255,
      255, 255, 255, 255,
    };
    gl->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    gl->BindTexture(GL_TEXTURE_2D, 0);
  }

  tex2::~tex2() {
    device_->immediate_context()->gl()->DeleteTextures(1, &texture_);
  }

  void tex2::bind(Context_ptr const& ctx) {
    context_ = ctx;
    context_->gl()->BindTexture(GL_TEXTURE_2D, texture_);
  }

  void tex2::unbind() {
    context_->gl()->BindTexture(GL_TEXTURE_2D, 0);
    context_.reset();
  }

  int tex2::width() const {
    return width_;
  }

  int tex2::height() const {
    return height_;
  }

  void tex2::copy_from(const void* buffer) {
    if(context_) {
      context_->gl()->TexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width_, height_, 0, GL_BGRA, GL_UNSIGNED_BYTE, buffer);
    }
  }

  void *tex2::ovr_handle() const {
    return (void*)(uintptr_t)texture_;
  }
}

