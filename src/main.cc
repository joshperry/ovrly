/* 
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 * 
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 */
#include <sstream>
#include "platform.h"

#include "include/cef_sandbox_win.h"
#include "ovrly_app.h"

#include "openvr.h"

//
// Flag selection of high-performance gpu mode for NV and AMD
//
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

void InitOpenVR() {
  OutputDebugString(L"OPENVR INITIALIZING");
  vr::HmdError m_eLastHmdError = vr::VRInitError_None;
  vr::IVRSystem *pVRSystem = vr::VR_Init( &m_eLastHmdError, vr::VRApplication_Overlay );

  if(m_eLastHmdError != vr::VRInitError_None) {
    OutputDebugString(L"OPENVR FAIL Init");
  } else {
    OutputDebugString(L"OPENVR Initialized!");

	char buf[128];
	vr::TrackedPropertyError err;
	pVRSystem->GetStringTrackedDeviceProperty(vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_SerialNumber_String, buf, sizeof(buf), &err);
	if(err != vr::TrackedProp_Success) {
      OutputDebugString(L"OPENVR FAIL get tracked dev");
	} else {
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

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int) {

  CefEnableHighDPISupport();

  // Use CEF to parse command-line arguments
  CefMainArgs main_args(hInstance);

  // Multi-process reentrant dispatch
  void* sandbox_info = nullptr;
  int exit_code = CefExecuteProcess(main_args, nullptr, sandbox_info);
  if (exit_code >= 0) {
    // Child process completed, gbye
    return exit_code;
  }

  // Test OpenVR initialization
  InitOpenVR();

  // Setup CEF settings
  CefSettings settings;
  settings.no_sandbox = true;

  // Create app instance to bind to CEF handlers
  CefRefPtr<OvrlyApp> app(new OvrlyApp);

  // Bind settings and app into CEF init
  CefInitialize(main_args, settings, app.get(), sandbox_info);

  // Pump the CEF message loop until CefQuitMessageLoop() is called
  CefRunMessageLoop();

  CefShutdown();

  return 0;
}
