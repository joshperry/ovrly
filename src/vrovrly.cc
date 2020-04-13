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
#include "openvr.h"

#include "appovrly.h"

namespace ovr = ::vr;

// Module exports
namespace ovrly{ namespace vr{
// Module local
namespace {

  void initVR() {
    OutputDebugString(L"OPENVR INITIALIZING");
    ovr::HmdError m_eLastHmdError = ovr::VRInitError_None;
    ovr::IVRSystem* pVRSystem = ovr::VR_Init(&m_eLastHmdError, ovr::VRApplication_Overlay);

    if (m_eLastHmdError != ovr::VRInitError_None) {
      OutputDebugString(L"OPENVR FAIL Init");
    }
    else {
      OutputDebugString(L"OPENVR Initialized!");

      char buf[128];
      ovr::TrackedPropertyError err;
      pVRSystem->GetStringTrackedDeviceProperty(ovr::k_unTrackedDeviceIndex_Hmd, ovr::Prop_SerialNumber_String, buf, sizeof(buf), &err);
      if (err != ovr::TrackedProp_Success) {
        OutputDebugString(L"OPENVR FAIL get tracked dev");
      }
      else {
        std::wstringstream ss;
        ss << L"OPENVR got tracked dev: " << buf;
        OutputDebugString(ss.str().c_str());
      }
    }
    /*
      if(vr::VROverlay()) {
        OutputDebugString(L"hi");
      }
    */
  }

  void onBrowserProcess(process::Browser& browser) {
    initVR();
  }

} // module local

void registerHooks() {
  // Register for notification when this is a browser process
  process::OnBrowser.attach(onBrowserProcess);
}

}} // module exports
