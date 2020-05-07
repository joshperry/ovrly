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

#include "appovrly.h"
#include "logging.h"
#include "ranges.hpp"

namespace ovr = ::vr;
using namespace Range;
using json = nlohmann::json;


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
  std::thread* loop_ = nullptr;

  // Set of openvr tracked device information
  std::vector<std::unique_ptr<json>> devices_;

  std::unique_ptr<json> createDevice(int slot) {
    auto vr = ovr::VRSystem();
    auto pdevice = std::make_unique<json>();
    auto& device = *pdevice;

    // Allocate error for out-params when getting device props
    ovr::TrackedPropertyError err;

    // Specialized device properties
    switch(ovr::VRSystem()->GetTrackedDeviceClass(slot))
    {
    case ovr::TrackedDeviceClass_HMD:
    {
      logger::info("OPENVR found hmd");

      device["type"] = "hmd";

      auto style = static_cast<ovr::EHmdTrackingStyle>(vr->GetInt32TrackedDeviceProperty(slot, ovr::Prop_HmdTrackingStyle_Int32, &err));
      if(err == ovr::ETrackedPropertyError::TrackedProp_Success) {
        auto found = trackstylemap.find(style);
        if(trackstylemap.end() != found) {
          device["tracking"] = found->second;
        } else {
          device["tracking"] = "unknown";
        }
      }
    }
    break;

    case ovr::TrackedDeviceClass_Controller:
    {
      logger::info("OPENVR found controller");

      device["type"] = "controller";

      auto role = vr->GetControllerRoleForTrackedDeviceIndex(slot);
      auto found = rolemap.find(role);
      if(rolemap.end() != found) {
        device["role"] = found->second;
      } else {
        device["role"] = "invalid";
      }
    }
    break;

    case ovr::TrackedDeviceClass_GenericTracker:
      logger::info("OPENVR found generic tracker");
      device["type"] = "tracker";
      break;
      
    case ovr::TrackedDeviceClass_TrackingReference:
      logger::info("OPENVR found tracking reference");
      device["type"] = "reference";
      break;

    // Untracked devices
    default:
      return nullptr;
    }

    // Generic device properties
    device["slot"] = slot;
    // A device always occupies the same slot for an application, if the device
    // is lost after being enumerated by openvr, it will be shown as disconnected.
    device["connected"] = vr->IsTrackedDeviceConnected(slot);

    std::string buf; // temp buffer to hold property strings
    buf.reserve(ovr::k_unMaxPropertyStringSize);

    vr->GetStringTrackedDeviceProperty(slot, ovr::Prop_ManufacturerName_String, buf.data(), ovr::k_unMaxPropertyStringSize, &err);
    if(err == ovr::ETrackedPropertyError::TrackedProp_Success) { device["manufacturer"] = buf; }

    vr->GetStringTrackedDeviceProperty(slot, ovr::Prop_ModelNumber_String, buf.data(), ovr::k_unMaxPropertyStringSize, &err);
    if(err == ovr::ETrackedPropertyError::TrackedProp_Success) { device["model"] = buf; }

    vr->GetStringTrackedDeviceProperty(slot, ovr::Prop_SerialNumber_String, buf.data(), ovr::k_unMaxPropertyStringSize, &err);
    if(err == ovr::ETrackedPropertyError::TrackedProp_Success) { device["serial"] = buf; }

    return pdevice;
  }

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
    for(auto i: range(ovr::k_unMaxTrackedDeviceCount)) {
      // Create and store device instances for each
      std::unique_ptr<json> device = createDevice(i);
      if(device) {
        devices_.push_back(std::move(device));
      }
    }

    // Notify listeners that the vr module is initialized
    OnReady();

    // VR event dispatch loop
    loop_ = new std::thread([vrsys]() {
      ovr::VREvent_t event;
      ovr::TrackedDevicePose_t poses[ovr::k_unMaxTrackedDeviceCount];
      while(true) {
        // Get the next event if there is one
        if (ovr::VRSystem()->PollNextEvent(&event, sizeof(event))) {
          logger::debug("OPENVR event ({}): {}", event.trackedDeviceIndex, ovr::VRSystem()->GetEventTypeNameFromEnum(static_cast<ovr::EVREventType>(event.eventType)));

          // Handle device add/remove events
          switch(event.eventType) {
          // Device added
          case ovr::EVREventType::VREvent_TrackedDeviceActivated:
          {
            auto device = createDevice(event.trackedDeviceIndex);
            if(device) {
              logger::debug("OPENVR added activated device");
              devices_.push_back(std::move(device));
            }
          }
          break;

          // Device removed
          case ovr::EVREventType::VREvent_TrackedDeviceDeactivated:
            logger::debug("OPENVR remove deactivated device");
            devices_.erase(
              std::remove_if(devices_.begin(), devices_.end(), [event](auto& d) { return (*d)["slot"] == event.trackedDeviceIndex; }),
              devices_.end()
            );
            break;
          }

          continue; // Continue loop until no events are pending
        }

        // Get the current set of device poses
        ovr::VRSystem()->GetDeviceToAbsoluteTrackingPose(ovr::ETrackingUniverseOrigin::TrackingUniverseStanding, 0, poses, ovr::k_unMaxTrackedDeviceCount);
        // Add pose to device
        for(auto& pd: devices_) {
          auto& device = *pd;
          auto& pose = poses[device["slot"]];

          device["pose"] = {
            { "position", pose.mDeviceToAbsoluteTracking.m },
            { "velocity", pose.vVelocity.v },
            { "angular", pose.vAngularVelocity.v },
            { "isvalid", pose.bPoseIsValid },
            { "result", pose.eTrackingResult },
          };

          device["connected"] = pose.bDeviceIsConnected;
        }

        // TODO: Get controller input states

        // Dispatch device update observable to notify listeners
        OnDevicesUpdated(devices_);

        // Wait to loop next, ~60fps
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
      }
    });
  }

  // A single graphics context to share between all overlays
  std::shared_ptr<d3d::Device> d3dev_;

  void onBrowserProcess(process::Browser& browser) {
    // Init VR on the browser main thread, all VR ops ahould happen on this thread
    // also serializes creation of overlays until both the browser and VR stacks are ready
    browser.SubOnContextInitialized.attach([]() {
      d3dev_ = d3d::create_device();
      initVR();
    });
  }

} // module local


/*
 * Module exports
 */

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

Event<const std::vector<std::unique_ptr<nlohmann::json>>&> OnDevicesUpdated;

const std::vector<std::unique_ptr<nlohmann::json>> &getDevices() {
  return devices_;
}

void registerHooks() {
  // Register for notification when this is a browser process
  process::OnBrowser.attach(onBrowserProcess);
}

}} // module exports
