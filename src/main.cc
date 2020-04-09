/* 
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 * 
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 */
#include "platform.h"

#include "include/cef_sandbox_win.h"
#include "ovrly_app.h"

//
// Flag selection of high-performance gpu mode for NV and AMD
//
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
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
