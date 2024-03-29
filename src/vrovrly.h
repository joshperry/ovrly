/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#pragma once

#include <optional>
#include "openvr.h"
#include "mathfu/glsl_mappings.h"

#include "events.h"

#include "gfx.h"

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

 /**
 * A base class that represents the concept of an overlay running in the 3d space
 * of the user's virtual environment.
 *
 * Encapsulates logic for subclass interaction with the VR API.
 */
  class Overlay {
    public:
      Overlay(const std::string &name, mathfu::vec2 size);
      virtual ~Overlay();

      /**
       * Logical size of the overlay
       *
       * x is the width of the overlay in meters
       * y is the aspect ratio of the width to the height
       */
      mathfu::vec2 size() { return size_; }

      /**
       * Sets the transform matrix for positioning the overlay in VR space
       */
      void setTransform(const mathfu::mat4 &matrix);

      /**
       * Gets the overlay's current positioning transform matrix
       */
      const mathfu::mat4 getTransform();

      /**
       * Parents the positioning of this overlay to another overlay with a transform, visibility is also mirrored
       */
      void setParent(const Overlay &parent, const mathfu::mat4 &matrix);

    protected:
      /**
       * For subclasses to provide a paint buffer for overlay rendering along
       * with a list of dirty rectangles.
       *
       * Only buffers with 32-bit pixels are supported, in one of the formats
       * specified in `gfx::BufferFormat`.
       */
      void render(const void *buffer, const std::vector<mathfu::recti> &dirty);

      /**
       * Renders an image file to the overlay texture
       */
      void renderImageFile(const std::string &path);

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
       * For subclasses to notify of a change in render target size and the bounds as as u,v.
       *
       * This must be called before `render()` is called with a buffer of the
       * new target size.
       *
       * Bounds is a tuple holding a minUV and maxUV vector for specifying what
       * portion of the target should be rendered. This is also used in cases
       * like opengl where the bottom right of the texture is considered the
       * start (e.g. `{{0, 1}, {1, 0}}`).
       */
      void updateTargetSize(mathfu::vec2i size, const std::tuple<mathfu::vec2, mathfu::vec2> &bounds);

    private:
      mathfu::vec2 size_;
      ::gfx::tex2_ptr texture_;
      ::vr::Texture_t vrtexture_;
      ::vr::VROverlayHandle_t vroverlay_;
      ::vr::HmdMatrix34_t transform_{ { {1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0} } };
      ::vr::VROverlayHandle_t parent_{ ::vr::k_ulOverlayHandleInvalid };
  };

  /**
   * An abstraction for a VR devices position in the 3D play area
   */
  struct DevicePose {
    DevicePose() {}
    DevicePose(::vr::TrackedDevicePose_t pose);

    bool operator ==(const ::vr::TrackedDevicePose_t& b) const;

    mathfu::mat4 matrix;
    mathfu::vec3 velocity;
    mathfu::vec3 angular;
    bool valid{ false };
    ::vr::ETrackingResult result{ ::vr::ETrackingResult::TrackingResult_Uninitialized };
  };

  /**
   * An abstraction for the VR API's tracked devices
   */
  struct TrackedDevice {
    TrackedDevice() {}
    TrackedDevice(unsigned slot);

    // HMD type properties
    std::optional<::vr::EHmdTrackingStyle> trackingStyle; // Method used to track devices in the physical space

    // Controller type properties
    std::optional<::vr::ETrackedControllerRole> role; // The hand or subtype (e.g. treadmill) an input controller fills

    // Properties common to all device types
    unsigned slot{ 0 }; // Fixed identifier for a tracked device in a single session
    ::vr::TrackedDeviceClass type{ ::vr::ETrackedDeviceClass::TrackedDeviceClass_Invalid }; // The type of device being tracked
    bool connected{ false }; // Flags device connected state while keeping its assigned slot

    std::optional<DevicePose> pose; // Position, rotation, and velocity information

    std::wstring manufacturer;
    std::wstring model;
    std::wstring serial;
  };


  /** Raised when the VR system is initialized and ready to go */
  extern Event<> OnReady;

  /** Raised when device state is updated from the VR system */
  extern Event<std::shared_ptr<const std::vector<TrackedDevice>>> OnDevicesUpdated;

  /** Gets a list of devices states currently known by the VR system */
  const std::vector<TrackedDevice> &getDevices();

  const ::vr::HmdQuad_t getPlaybounds();

  /**
   * Registers to launch the vr event thread once the browser process is initialized
   */
  void registerHooks();

}} // namespaces

