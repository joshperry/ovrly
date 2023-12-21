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
#include <algorithm>
#include <iostream>
#include <string>
#include <locale>
#include <codecvt>
#include <ranges>

#include "appovrly.h"
#include "logging.h"

namespace ovr = ::vr;
using namespace std::ranges;


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
    for(auto i: std::views::iota(0u, ovr::k_unMaxTrackedDeviceCount)) {
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
      int binding_reloaded = 0;
      ovr::VREvent_t event;
      ovr::TrackedDevicePose_t poses[ovr::k_unMaxTrackedDeviceCount];
      while(!done_) {
        // Get the next event if there is one
        if (ovr::VRSystem()->PollNextEvent(&event, sizeof(event))) {

          // Handle device add/remove events
          switch(event.eventType) {
          // Device added
          case ovr::EVREventType::VREvent_TrackedDeviceActivated:
          {
            // See if this is a device we've seen already
            auto it = std::find_if(devices_.begin(), devices_.end(),
                [&event](const TrackedDevice &dev){ return event.trackedDeviceIndex == dev.slot; });
            if(it == devices_.end()) {
              logger::debug("(vr) added activated device {}", event.trackedDeviceIndex);
              devices_.push_back(TrackedDevice(event.trackedDeviceIndex));
              maxslot = std::max(maxslot, event.trackedDeviceIndex);
            }
          }
          break;
          case ovr::EVREventType::VREvent_PropertyChanged:
          {
            // See if this is a device we've seen already
            auto it = std::find_if(devices_.begin(), devices_.end(),
                [&event](const TrackedDevice &dev){ return event.trackedDeviceIndex == dev.slot; });

            if(it != devices_.end()) {
              auto fresh = TrackedDevice(it->slot);
              logger::debug("(vr) device property changed {}", it->slot);
              *it = fresh;
            }
          }
          break;

          // Device removed
          case ovr::EVREventType::VREvent_TrackedDeviceDeactivated:
          {
            logger::debug("(vr) deactivated device {}", event.trackedDeviceIndex);
            /*
            // The pose query should update the device to connected = false
            std::erase_if(devices_, [&event](const TrackedDevice &dev){ return event.trackedDeviceIndex == dev.slot; });
            auto max = std::max_element(devices_.begin(), devices_.end(),
                [](const auto &lhs, const auto &rhs) { return lhs.slot < rhs.slot; });
            maxslot = max->slot;
            */
          }
          break;

          case ovr::EVREventType::VREvent_ActionBindingReloaded:
            if(++binding_reloaded%1000 == 0)
              logger::debug("(vr) reloaded {}", binding_reloaded);
            break;

          default:
            logger::debug("(vr) event skipped ({}): {}", event.trackedDeviceIndex, ovr::VRSystem()->GetEventTypeNameFromEnum(static_cast<ovr::EVREventType>(event.eventType)));
            break;
          }

          continue; // Continue loop until no events are pending
        }

        // Get the current set of device poses
        ovr::VRSystem()->GetDeviceToAbsoluteTrackingPose(ovr::ETrackingUniverseOrigin::TrackingUniverseStanding, 0, poses, maxslot+1);
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

        // Wait to loop next
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
      }
    });
  }

  // A single graphics context to share between all overlays
  gfx::device_ptr gfxdev_;

  void onBrowserProcess(process::Browser& browser) {
    // Init VR on the browser main thread, all events should be raised on this thread
    // also serializes creation of overlies until both the browser and VR stacks are ready
    browser.SubOnContextInitialized.attach([]() {
      gfxdev_ = std::move(gfx::create_device());
      initVR();
    });
  }

  // HMD Matrix is right-handed system [row][column]
  // +y is up
  // +x is to the right
  // -z is forward
  // Distance unit is  meters
  // Mathfu matrix constructor/accessor is column-major
  mathfu::mat4 to_mathfu(const ::vr::HmdMatrix34_t &mat) {
    return {
      mat.m[0][0], mat.m[1][0], mat.m[2][0], 0,
      mat.m[0][1], mat.m[1][1], mat.m[2][1], 0,
      mat.m[0][2], mat.m[1][2], mat.m[2][2], 0,
      mat.m[0][3], mat.m[1][3], mat.m[2][3], 0
    };
  }

  ::vr::HmdMatrix34_t from_mathfu(const mathfu::mat4 &mat) {
    return { {
      {mat[0], mat[4], mat[8], mat[12]},
      {mat[1], mat[5], mat[9], mat[13]},
      {mat[2], mat[6], mat[10], mat[14]}
    } };
  }

} // module local


/*
 * Module exports
 */

DevicePose::DevicePose(ovr::TrackedDevicePose_t pose) :
  matrix(to_mathfu(pose.mDeviceToAbsoluteTracking)),
  velocity(pose.vVelocity.v),
  angular(pose.vAngularVelocity.v),
  valid(pose.bPoseIsValid),
  result(pose.eTrackingResult)
{ }

bool DevicePose::operator ==(const ::vr::TrackedDevicePose_t& b) const {
  auto &nmat = b.mDeviceToAbsoluteTracking.m;
  if(
    matrix.data_[0].data_[0] != nmat[0][0] ||
    matrix.data_[0].data_[1] != nmat[0][1] ||
    matrix.data_[0].data_[2] != nmat[0][2] ||
    matrix.data_[0].data_[3] != nmat[0][3] ||
    matrix.data_[1].data_[0] != nmat[1][0] ||
    matrix.data_[1].data_[1] != nmat[1][1] ||
    matrix.data_[1].data_[2] != nmat[1][2] ||
    matrix.data_[1].data_[3] != nmat[1][3] ||
    matrix.data_[2].data_[0] != nmat[2][0] ||
    matrix.data_[2].data_[1] != nmat[2][1] ||
    matrix.data_[2].data_[2] != nmat[2][2] ||
    matrix.data_[2].data_[3] != nmat[2][3] ||
    !memcmp(&velocity.data_[0], &b.vVelocity.v[0], sizeof(float)*3) ||
    !memcmp(&angular.data_[0], &b.vAngularVelocity.v[0], sizeof(float)*3) ||
    valid != b.bPoseIsValid ||
    result != b.eTrackingResult
  ) { return false; }

  return true;
}

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
  default:
    logger::info("OPENVR unhandled tracking event");
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

Overlay::Overlay(const std::string &name, mathfu::vec2 size) :
  size_(size),
  vroverlay_(ovr::k_ulOverlayHandleInvalid)
{
  // Get the OpenVR Overlay interface
  auto ovrl = ovr::VROverlay();
  if(ovrl) {
    // First see if the overlay exists already
    // TODO: Derive a stable name for overlays
    auto err = ovrl->FindOverlay(name.c_str(), &vroverlay_);
    if(err != ovr::VROverlayError_None) {
      // Didn't find an existing overlay, create a new one
      logger::debug("(vr) creating overlay");
      err = ovrl->CreateOverlay(name.c_str(), name.c_str(), &vroverlay_);
      if (err != ovr::VROverlayError_None) {
        logger::error("OPENVR error creating overlay {}", err);
      }
    }

    // Set overlay properties and show it
    if(vroverlay_ != ovr::k_ulOverlayHandleInvalid) {
      logger::debug("(vr) overlay created, setting width to {}m and showing", size.x);
      ovrl->SetOverlayWidthInMeters(vroverlay_, size.x);

      ovrl->ShowOverlay(vroverlay_);
    } else {
      logger::error("OPENVR overlay creation failed, invalid handle returned");
    }
  } else {
    logger::error("OPENVR overlay interface unavailable");
  }

  // Set the openvr static texture definition info
  vrtexture_.eType = gfx::TextureType;
  vrtexture_.eColorSpace = ovr::ColorSpace_Gamma;
}

Overlay::~Overlay() {
  if(vroverlay_ != ovr::k_ulOverlayHandleInvalid) {
    ovr::VROverlay()->DestroyOverlay(vroverlay_);
  }
}

void Overlay::setTransform(const mathfu::mat4 &matrix) {
  transform_ = from_mathfu(matrix);
  ovr::VROverlay()->SetOverlayTransformAbsolute(vroverlay_, ovr::ETrackingUniverseOrigin::TrackingUniverseStanding, &transform_);
}

const mathfu::mat4 Overlay::getTransform() {
  ovr::HmdMatrix34_t xform;
  ovr::ETrackingUniverseOrigin origin;
  ovr::VROverlay()->GetOverlayTransformAbsolute(vroverlay_, &origin, &xform);
  return to_mathfu(xform);
}

void Overlay::setParent(const Overlay& parent, const mathfu::mat4 &matrix) {
  transform_ = from_mathfu(matrix);
  parent_ = parent.vroverlay_;
  assert(false);
  // FIXME: Removed from OpenVR, not sure if we reimplement
  // https://github.com/ValveSoftware/openvr/commit/751538d6cbde9d060e5906c1d45c8cedeaf0ee18
  // ovr::VROverlay()->SetOverlayTransformOverlayRelative(vroverlay_, parent_, &transform_);
}

void Overlay::render(const void* buffer, const std::vector<mathfu::recti> &dirty) {
  if(vroverlay_ == ovr::k_ulOverlayHandleInvalid) {
    logger::error("OPENVR Overlay::render with no overlay");
    return;
  }

  // TODO: Handle dirty rect optimization

  // Bind and copy data from the chromium paint buffer to the texture
  gfx::ScopedBinder<gfx::tex2> binder(gfxdev_, texture_);
  texture_->copy_from(buffer);

  // Notify openvr of the texture
  // TODO: Handle errors
  auto err = ovr::VROverlay()->SetOverlayTexture(vroverlay_, &vrtexture_);
  if(err != ovr::VROverlayError_None) {
    logger::debug("!!!!(vr) Error setting overlay texture {}", err);
  }
}

void Overlay::renderImageFile(const std::string &path) {
  auto err = ovr::VROverlay()->SetOverlayFromFile(vroverlay_, path.c_str());
  if(err != ovr::VROverlayError_None) {
    logger::debug("!!!!(vr) Error setting overlay contents to file {}", err);
  }
}

/**
 * When the impl class gets a new logical layout size, it should decide the
 * pixel size it wants to render and call this to update the target size.
 */
void Overlay::updateTargetSize(mathfu::vec2i size, const std::tuple<mathfu::vec2, mathfu::vec2> &bounds) {
  // Create a new chromium-compatible(BGRA32) D3D/GL texture of the correct dims
  auto newtex = gfxdev_->create_texture(size.x, size.y);
  // Point the openvr texture descriptor at the d3d/GL texture
  vrtexture_.handle = newtex->ovr_handle();
  texture_ = newtex;

  ovr::VRTextureBounds_t vrbounds;
  vrbounds.uMin = std::get<0>(bounds).x;
  vrbounds.uMax = std::get<0>(bounds).y;
  vrbounds.vMin = std::get<1>(bounds).x;
  vrbounds.vMax = std::get<1>(bounds).y;
  ovr::VROverlay()->SetOverlayTextureBounds(vroverlay_, &vrbounds);
}

Event<> OnReady;

Event<std::shared_ptr<const std::vector<TrackedDevice>>> OnDevicesUpdated;

const std::vector<TrackedDevice> &getDevices() {
  return devices_;
}

const ::vr::HmdQuad_t getPlaybounds() {
  ::vr::EVRInitError eError = ::vr::VRInitError_None;
  ::vr::IVRChaperone* pChaperone = static_cast<::vr::IVRChaperone*>(::vr::VR_GetGenericInterface(::vr::IVRChaperone_Version, &eError));
  if (!pChaperone) {
    logger::info("Failed to obtain Chaperone interface: {}", ::vr::VR_GetVRInitErrorAsEnglishDescription(eError));
    return {};
  }

  ::vr::HmdQuad_t playAreaRect;
  bool hasPlayArea = pChaperone->GetPlayAreaRect(&playAreaRect);

  if(hasPlayArea)
    return playAreaRect;
  else
    return {};
}

void registerHooks() {
  // Register for notification when this is a browser process
  process::OnBrowser.attach(onBrowserProcess);
}

}} // module exports
