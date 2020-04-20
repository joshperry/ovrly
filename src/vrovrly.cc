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
#include "ranges.hpp"

#include "appovrly.h"

namespace ovr = ::vr;
using namespace Range;


namespace ovrly{ namespace vr{

// Module local
namespace {

  std::thread* loop_ = nullptr;
  std::vector<std::unique_ptr<Device>> devices_;

  void initVR() {
    OutputDebugString(L"OPENVR INITIALIZING");
    ovr::EVRInitError initerr = ovr::VRInitError_None;
    ovr::IVRSystem* vrsys = ovr::VR_Init(&initerr, ovr::VRApplication_Overlay);

    if (initerr != ovr::VRInitError_None) {
      std::wstringstream ss;
      ss << L"OPENVR FAIL Init: " << ovr::VR_GetVRInitErrorAsEnglishDescription(initerr);
      OutputDebugString(ss.str().c_str());

      return;
    }

    OutputDebugString(L"OPENVR Initialized!");

    /** Enumerate tracked VR devices */
    for(auto i: range(ovr::k_unMaxTrackedDeviceCount)) {
      auto dclass = vrsys->GetTrackedDeviceClass(i);

      // Don't enumerate invalid device slots
      if(dclass == ovr::TrackedDeviceClass_Invalid) continue;

      // Create device instace for the specific class
      std::unique_ptr<Device> device;
      switch(dclass)
      {
      case ovr::TrackedDeviceClass_HMD:
        OutputDebugString(L"OPENVR found hmd\n");
        device = std::make_unique<HMD>(i);
        break;
      case ovr::TrackedDeviceClass_Controller:
        OutputDebugString(L"OPENVR found controller\n");
        device = std::make_unique<Controller>(i);
        break;
      case ovr::TrackedDeviceClass_GenericTracker:
        OutputDebugString(L"OPENVR found generic tracker\n");
        device = std::make_unique<Tracker>(i);
        break;
      case ovr::TrackedDeviceClass_TrackingReference:
        OutputDebugString(L"OPENVR found tracking reference\n");
        device = std::make_unique<Reference>(i);
        break;
      }

      devices_.push_back(std::move(device));
    }

    OnReady();

    loop_ = new std::thread([]() {
      while (true) {
        ovr::VREvent_t event;
        ovr::TrackedDevicePose_t pose;
        if (ovr::VRSystem()->PollNextEventWithPose(ovr::ETrackingUniverseOrigin::TrackingUniverseStanding, &event, sizeof(event), &pose)) {
          std::wstringstream ss;
          ss << L"OPENVR event (" << event.trackedDeviceIndex << L"): " << ovr::VRSystem()->GetEventTypeNameFromEnum(static_cast<ovr::EVREventType>(event.eventType)) << std::endl;
          OutputDebugString(ss.str().c_str());
          continue;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(15));
      }
    });
    // TODO: Implement VR event dispatch loop
  }

  // A single graphics context to share between all overlays
  std::shared_ptr<d3d::Device> device_;

  void onBrowserProcess(process::Browser& browser) {
    // Init VR on the browser main thread, all VR ops ahould happen on this thread
    // also serializes creation of overlays until both the browser and VR stacks are ready
    browser.SubOnContextInitialized.attach([]() {
      device_ = d3d::create_device();
      initVR();
    });
  }

} // module local


/*
 * Module exports
 */

Device::Device(int slot) : slot_(slot) {
  auto vr = ovr::VRSystem();

  // A device always occupies the same slot for an application, if the device
  // is lost after being enumerated by openvr, it will be shown as disconnected.
  connected_ = vr->IsTrackedDeviceConnected(slot);

  std::wstringstream ss;
  ss << L"OPENVR found tracked device in slot " << slot << L" that is " << (connected_ ? L"connected" : L"not connected") << std::endl;
  OutputDebugString(ss.str().c_str());

  // Allocate error for out-params when getting device props
  ovr::TrackedPropertyError err;
  char buf[ovr::k_unMaxPropertyStringSize]; // temp buffer to hold property strings

  vr->GetStringTrackedDeviceProperty(slot, ovr::Prop_ManufacturerName_String, buf, sizeof(buf), &err);
  if(err == ovr::ETrackedPropertyError::TrackedProp_Success) { mfg_ = buf; }

  vr->GetStringTrackedDeviceProperty(slot, ovr::Prop_ModelNumber_String, buf, sizeof(buf), &err);
  if(err == ovr::ETrackedPropertyError::TrackedProp_Success) { model_ = buf; }

  vr->GetStringTrackedDeviceProperty(slot, ovr::Prop_SerialNumber_String, buf, sizeof(buf), &err);
  if(err == ovr::ETrackedPropertyError::TrackedProp_Success) { serial_ = buf; }
}

Device::~Device() {

}

HMD::HMD(int slot) : Device(slot) {
  auto vr = ovr::VRSystem();

  ovr::TrackedPropertyError err;

  tracking_ = "unknown";
  int style = vr->GetInt32TrackedDeviceProperty(slot, ovr::Prop_HmdTrackingStyle_Int32, &err);
  if(err == ovr::ETrackedPropertyError::TrackedProp_Success) {
    switch(style) {
    case ovr::EHmdTrackingStyle::HmdTrackingStyle_Lighthouse:
      tracking_ = "lighthouse";
      break;
    case ovr::EHmdTrackingStyle::HmdTrackingStyle_OutsideInCameras:
      tracking_ = "outside-in";
      break;
    case ovr::EHmdTrackingStyle::HmdTrackingStyle_InsideOutCameras:
      tracking_ = "inside-out";
      break;
    }
  }
}

Controller::Controller(int slot) : Device(slot) {
  auto vr = ovr::VRSystem();

  auto role = vr->GetControllerRoleForTrackedDeviceIndex(slot);
  switch(role) {
  case ovr::ETrackedControllerRole::TrackedControllerRole_LeftHand:
    role_ = "left";
    break;
  case ovr::ETrackedControllerRole::TrackedControllerRole_RightHand:
    role_ = "left";
    break;
  case ovr::ETrackedControllerRole::TrackedControllerRole_OptOut:
    role_ = "optout";
    break;
  case ovr::ETrackedControllerRole::TrackedControllerRole_Stylus:
    role_ = "stylus";
    break;
  case ovr::ETrackedControllerRole::TrackedControllerRole_Treadmill:
    role_ = "treadmill";
    break;
  default:
    role_ = "invalid";
    break;
  }
}

Tracker::Tracker(int slot) : Device(slot) {
}

Reference::Reference(int slot) : Device(slot) {
}

Overlay::Overlay(mathfu::vec2 size, BufferFormat format) : size_(size), texformat_(format) {
  auto ovrly = ovr::VROverlay();
  if (ovrly) {
    // First see if the overlay exists already
    // TODO: Derive a stable name for overlays
    auto err = ovrly->FindOverlay("ovrlymain", &vroverlay_);
    if (err != ovr::VROverlayError_None) {
      err = ovrly->CreateOverlay("ovrlymain", "Main ovrly", &vroverlay_);
      if (err != ovr::VROverlayError_None) {
        std::wstringstream ss;
        ss << L"OVRLY VR error creating overlay" << err;
        OutputDebugString(ss.str().c_str());
      }
    }

    if (vroverlay_ != ovr::k_ulOverlayHandleInvalid) {
      ovrly->SetOverlayWidthInMeters(vroverlay_, 1.5f);
      ovrly->ShowOverlay(vroverlay_);
    }
  }
  else {
    OutputDebugString(L"OVRLY VR overlay interface unavailable");
  }

  // Set the openvr static texture definition info:w
  vrtexture_.eType = ovr::TextureType_DirectX;
  vrtexture_.eColorSpace = ovr::ColorSpace_Gamma;
}

Overlay::~Overlay() {
  if (vroverlay_ != ovr::k_ulOverlayHandleInvalid) {
    ovr::VROverlay()->DestroyOverlay(vroverlay_);
  }
}

void Overlay::render(const void* buffer, std::vector<mathfu::recti> &dirty) {
  if(vroverlay_ == ovr::k_ulOverlayHandleInvalid) {
    OutputDebugString(L"OVRLY Overlay::render with no overlay");
    return;
  }

  // TODO: Handle dirty rect optimization

  // Copy data from the chromium paint buffer to the texture
  d3d::ScopedBinder<d3d::Texture2D> binder(device_->immediate_context(), texture_);
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
  texture_ = device_->create_texture(size.x, size.y, static_cast<DXGI_FORMAT>(texformat_), nullptr, 0);
  // Point the openvr texture descriptor at the d3d texture
  vrtexture_.handle = texture_->texture_.get();
}

Event<> OnReady;

void registerHooks() {
  // Register for notification when this is a browser process
  process::OnBrowser.attach(onBrowserProcess);
}

}} // module exports
