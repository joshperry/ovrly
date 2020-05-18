/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#include "platform.h"
#include "vrovrly.h"

#include <sstream>
#include <thread>
#include <iostream>
#include <string>
#include <locale>
#include <codecvt>

#include "appovrly.h"
#include "logging.h"
#include "ranges.hpp"

namespace ovr = ::vr;
using namespace Range;


namespace ovrly{ namespace vr{

// Module local
namespace {
  // Map from tracking style enum to string
  std::map<ovr::EHmdTrackingStyle, std::string> trackstylemap = {
    {ovr::EHmdTrackingStyle::HmdTrackingStyle_Lighthouse, "lighthouse"},
    {ovr::EHmdTrackingStyle::HmdTrackingStyle_OutsideInCameras, "outside-in"},
    {ovr::EHmdTrackingStyle::HmdTrackingStyle_InsideOutCameras, "inside-out"},
  };

  // Map from controller role enum to string
  std::map<ovr::ETrackedControllerRole, std::string> rolemap = {
    {ovr::ETrackedControllerRole::TrackedControllerRole_LeftHand, "left"},
    {ovr::ETrackedControllerRole::TrackedControllerRole_RightHand, "right"},
    {ovr::ETrackedControllerRole::TrackedControllerRole_OptOut, "optout"},
    {ovr::ETrackedControllerRole::TrackedControllerRole_Stylus, "stylus"},
    {ovr::ETrackedControllerRole::TrackedControllerRole_Treadmill, "treadmill"},
  };

  // Thread for running the openvr event loop
  std::unique_ptr<std::thread> loop_;
  std::atomic<bool> done_;

  // Set of openvr tracked device information
  std::vector<TrackedDevice> devices_;

  // Maximum device slot seen so far
  unsigned maxslot;

  void initVR() {
    logger::info("OPENVR INITIALIZING");
    ovr::EVRInitError initerr = ovr::VRInitError_None;
    ovr::IVRSystem* vrsys = ovr::VR_Init(&initerr, ovr::VRApplication_Overlay);

    if (initerr != ovr::VRInitError_None) {
      logger::error("OPENVR FAIL Init: {}", ovr::VR_GetVRInitErrorAsEnglishDescription(initerr));
      return;
    }

    logger::info("OPENVR Initialized!");

    /** Enumerate initial state from tracked VR devices */
    for(unsigned i: range(ovr::k_unMaxTrackedDeviceCount)) {
      // Create and store device instances for each valid slot
      auto type = ovr::VRSystem()->GetTrackedDeviceClass(i);
      if(type != ovr::ETrackedDeviceClass::TrackedDeviceClass_Invalid) {
        devices_.push_back(TrackedDevice(i));
        maxslot = std::max(maxslot, i);
      }
    }

    // Notify listeners that the vr module is initialized
    OnReady();

    // VR event dispatch loop
    loop_ = std::make_unique<std::thread>([vrsys]() {
      ovr::VREvent_t event;
      ovr::TrackedDevicePose_t poses[ovr::k_unMaxTrackedDeviceCount];
      while(!done_) {
        // Get the next event if there is one
        if (ovr::VRSystem()->PollNextEvent(&event, sizeof(event))) {
          logger::debug("OPENVR event ({}): {}", event.trackedDeviceIndex, ovr::VRSystem()->GetEventTypeNameFromEnum(static_cast<ovr::EVREventType>(event.eventType)));

          // Handle device add/remove events
          switch(event.eventType) {
          // Device added
          case ovr::EVREventType::VREvent_TrackedDeviceActivated:
          {
            logger::debug("OPENVR added activated device");
            devices_.push_back(TrackedDevice(event.trackedDeviceIndex));
            maxslot = std::max(maxslot, event.trackedDeviceIndex);
          }
          break;

          // Device removed
          case ovr::EVREventType::VREvent_TrackedDeviceDeactivated:
            logger::debug("OPENVR remove deactivated device");
            // The pose query should update the device to connected = false

            /*
            auto deviceit = find_if(devices_.begin(), devices_.end(), [event](auto& d) { return d->slot == event.trackedDeviceIndex; });
            if(deviceit != devices_.end()) {
              (*deviceit)->connected = false;
            }
            */
            break;
          }

          continue; // Continue loop until no events are pending
        }

        // Get the current set of device poses
        ovr::VRSystem()->GetDeviceToAbsoluteTrackingPose(ovr::ETrackingUniverseOrigin::TrackingUniverseStanding, 0, poses, maxslot);
        // Add/update device pose and connected state
        for(auto& pd: devices_) {
          auto& pose = poses[pd.slot];
          pd.pose = DevicePose(pose);
          pd.connected = pose.bDeviceIsConnected;
        }

        // TODO: Get controller input states

        // Dispatch device update observable to notify listeners
        process::runOnMain([devices = std::make_shared<const std::vector<TrackedDevice>>(devices_)]() {
          OnDevicesUpdated(devices);
        });

        // Wait to loop next, ~60fps
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
      }
    });
  }

  // A single graphics context to share between all overlays
  std::shared_ptr<d3d::Device> d3dev_;

  void onBrowserProcess(process::Browser& browser) {
    // Init VR on the browser main thread, all events should be raised on this thread
    // also serializes creation of overlies until both the browser and VR stacks are ready
    browser.SubOnContextInitialized.attach([]() {
      d3dev_ = d3d::create_device();
      initVR();
    });
  }

} // module local


/*
 * Module exports
 */

DevicePose::DevicePose(ovr::TrackedDevicePose_t pose) :
  matrix(
    pose.mDeviceToAbsoluteTracking.m[0][0],
    pose.mDeviceToAbsoluteTracking.m[0][1],
    pose.mDeviceToAbsoluteTracking.m[0][2],
    pose.mDeviceToAbsoluteTracking.m[0][3],
    pose.mDeviceToAbsoluteTracking.m[1][0],
    pose.mDeviceToAbsoluteTracking.m[1][1],
    pose.mDeviceToAbsoluteTracking.m[1][2],
    pose.mDeviceToAbsoluteTracking.m[1][3],
    pose.mDeviceToAbsoluteTracking.m[2][0],
    pose.mDeviceToAbsoluteTracking.m[2][1],
    pose.mDeviceToAbsoluteTracking.m[2][2],
    pose.mDeviceToAbsoluteTracking.m[2][3]
  ),
  velocity(pose.vVelocity.v),
  angular(pose.vAngularVelocity.v),
  valid(pose.bPoseIsValid),
  result(pose.eTrackingResult)
{ }

TrackedDevice::TrackedDevice(unsigned slot) : slot(slot) {
  auto vr = ovr::VRSystem();

  // Allocate error for out-params when getting device props
  ovr::TrackedPropertyError err;

  type = ovr::VRSystem()->GetTrackedDeviceClass(slot);

  // Specialized device properties
  switch(type)
  {
  case ovr::TrackedDeviceClass_HMD:
    logger::info("OPENVR found hmd");
    trackingStyle = static_cast<ovr::EHmdTrackingStyle>(vr->GetInt32TrackedDeviceProperty(slot, ovr::Prop_HmdTrackingStyle_Int32, &err));
    break;

  case ovr::TrackedDeviceClass_Controller:
    logger::info("OPENVR found controller");
    role = vr->GetControllerRoleForTrackedDeviceIndex(slot);
    break;

  case ovr::TrackedDeviceClass_GenericTracker:
    logger::info("OPENVR found generic tracker");
    break;
    
  case ovr::TrackedDeviceClass_TrackingReference:
    logger::info("OPENVR found tracking reference");
    break;
  }

  // A device always occupies the same slot for an application, if the device
  // is lost after being enumerated by openvr, it will be shown as disconnected.
  connected = vr->IsTrackedDeviceConnected(slot);

  char buf[ovr::k_unMaxPropertyStringSize]; // temp buffer to hold property strings

  // Converter to change openvr utf8 to cef utf16 strings
  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

  vr->GetStringTrackedDeviceProperty(slot, ovr::Prop_ManufacturerName_String, buf, ovr::k_unMaxPropertyStringSize, &err);
  if(err == ovr::ETrackedPropertyError::TrackedProp_Success) { manufacturer = converter.from_bytes(buf); }

  vr->GetStringTrackedDeviceProperty(slot, ovr::Prop_ModelNumber_String, buf, ovr::k_unMaxPropertyStringSize, &err);
  if(err == ovr::ETrackedPropertyError::TrackedProp_Success) { model = converter.from_bytes(buf); }

  vr->GetStringTrackedDeviceProperty(slot, ovr::Prop_SerialNumber_String, buf, ovr::k_unMaxPropertyStringSize, &err);
  if(err == ovr::ETrackedPropertyError::TrackedProp_Success) { serial = converter.from_bytes(buf); }
}

Overlay::Overlay(mathfu::vec2 size, BufferFormat format) :
  vroverlay_(ovr::k_ulOverlayHandleInvalid),
  size_(size),
  texformat_(format)
{
  auto ovrl = ovr::VROverlay();
  if(ovrl) {
    // First see if the overlay exists already
    // TODO: Derive a stable name for overlays
    auto err = ovrl->FindOverlay("ovrlymain", &vroverlay_);
    if(err != ovr::VROverlayError_None) {
      // Didn't find an existing overlay, create a new one
      err = ovrl->CreateOverlay("ovrlymain", "Main ovrly", &vroverlay_);
      if (err != ovr::VROverlayError_None) {
        logger::error("OPENVR error creating overlay {}", err);
      }
    }

    // Set overlay properties and show it
    if(vroverlay_ != ovr::k_ulOverlayHandleInvalid) {
      ovrl->SetOverlayWidthInMeters(vroverlay_, size.x);
      ovrl->ShowOverlay(vroverlay_);
    }
  } else {
    logger::error("OPENVR overlay interface unavailable");
  }

  // Set the openvr static texture definition info
  vrtexture_.eType = ovr::TextureType_DirectX;
  vrtexture_.eColorSpace = ovr::ColorSpace_Gamma;
}

Overlay::~Overlay() {
  if(vroverlay_ != ovr::k_ulOverlayHandleInvalid) {
    ovr::VROverlay()->DestroyOverlay(vroverlay_);
  }
}

void Overlay::render(const void* buffer, std::vector<mathfu::recti> &dirty) {
  if(vroverlay_ == ovr::k_ulOverlayHandleInvalid) {
    logger::debug("OPENVR Overlay::render with no overlay");
    return;
  }

  // TODO: Handle dirty rect optimization

  // Copy data from the chromium paint buffer to the texture
  d3d::ScopedBinder<d3d::Texture2D> binder(d3dev_->immediate_context(), texture_);
  texture_->copy_from(
    buffer,
    texture_->width() * 4,
    texture_->height());

  // Notify openvr of the texture
  // TODO: Handle errors
  ovr::VROverlay()->SetOverlayTexture(vroverlay_, &vrtexture_);
}

void Overlay::updateTargetSize(mathfu::vec2i size) {
  // Create a new chromium-compatible(BGRA32) D3D texture of the correct dims
  texture_ = d3dev_->create_texture(size.x, size.y, static_cast<DXGI_FORMAT>(texformat_), nullptr, 0);
  // Point the openvr texture descriptor at the d3d texture
  vrtexture_.handle = texture_->texture_.get();
}

Event<> OnReady;

Event<std::shared_ptr<const std::vector<TrackedDevice>>> OnDevicesUpdated;

const std::vector<TrackedDevice> &getDevices() {
  return devices_;
}

void registerHooks() {
  // Register for notification when this is a browser process
  process::OnBrowser.attach(onBrowserProcess);
}

}} // module exports
