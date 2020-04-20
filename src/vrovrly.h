/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#pragma once

#include "openvr.h"
#include "mathfu/glsl_mappings.h"

#include "events.h"
#include "d3d.h"

/**
 * The purpose of this module is to interact with the vr API to initialize it and
 * mediate interactions with it on behalf of other modules.
 *
 * It produces events for state changes (add/remove) and positions of tracked
 * peripherals and their buttons in the VR universe, virtual mouse inputs, and
 * other state changes to the vr system in general.
 */

namespace ovrly { namespace vr {

  /** The pixel datatype that the overlay subclass provides for rendering */
  // FIXME: Hard dep on d3d
  enum class BufferFormat {
    RGBA = DXGI_FORMAT_R8G8B8A8_UNORM,
    BGRA = DXGI_FORMAT_B8G8R8A8_UNORM,
  };

  auto overlaydeleter = [&](::vr::VROverlayHandle_t handle) {
    ::vr::VROverlay()->DestroyOverlay(handle);
  };

 /**
 * A base class that represents the concept of an overlay running in the 3d space
 * of the user's virtual environment.
 *
 * Encapsulates logic for subclass interaction with the VR API.
 */
  class Overlay {
    public:
      Overlay(mathfu::vec2 size, BufferFormat format);
      virtual ~Overlay();

      /**
       * Logical size of the overlay
       *
       * x is the width of the overlay in meters
       * y is the aspect ratio of the width to the height
       */
      mathfu::vec2 size() { return size_; }

    protected:
      /**
       * For subclasses to provide a paint buffer for overlay rendering along
       * with a list of dirty rectangles.
       *
       * Only buffers with 32-bit pixels are supported, in one of the formats
       * specified in `BufferFormat`.
       */
      void render(const void *buffer, std::vector<mathfu::recti> &dirty);

      // TODO: Interface for shared texture rendering

      /**
       * For subclasses to be notified when the overlay layout
       * changes in a way that could impact render target size.
       *
       * When raised, the observed value will be equal to the return value of `size()`.
       *
       * The overlay subclass must not call `render()` before this event happens
       * for the first time.
       *
       * The subclass should use this logical layout to calculate a pixel size for
       * its render target, and must call `updateTargetSize()` when a layout change
       * causes a change in the render dimensions.
       */
      virtual void onLayout(mathfu::vec2) = 0;

      /**
       * For subclasses to notify of a change in render target size
       *
       * This must be called before `render()` is called with a buffer of the
       * new target size.
       */
      void updateTargetSize(mathfu::vec2i size);

    private:
      mathfu::vec2 size_;
      BufferFormat texformat_;
      std::shared_ptr<d3d::Texture2D> texture_;
      ::vr::Texture_t vrtexture_;
      ::vr::VROverlayHandle_t vroverlay_;
  };

  class Device {
    public:
      Device(int slot);
      virtual ~Device();

    private:
      int slot_;
      bool connected_;
      std::string mfg_;
      std::string model_;
      std::string serial_;
  };

  class HMD : public Device {
    public:
      HMD(int slot);

    private:
      std::string tracking_;
  };

  class Controller : public Device {
    public:
      Controller(int slot);

    private:
      std::string role_;
  };

  class Tracker : public Device {
    public:
      Tracker(int slot);
  };

  class Reference : public Device {
    public:
      Reference(int slot);
  };

  /** Raised when the VR system is initialized and ready to go */
  extern Event<> OnReady;

  /**
   * Registers to launch the vr event thread once the browser process is initialized
   */
  void registerHooks();

}} // namespaces

