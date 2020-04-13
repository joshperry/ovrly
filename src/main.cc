/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#include "platform.h"

#include "appovrly.h"
#include "uiovrly.h"
#include "jsovrly.h"
#include "vrovrly.h"

/**
 * Flag selection of high-performance gpu mode for NV and AMD hybrid setups
 */
extern "C"
{
	__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
	__declspec(dllexport) int AmdPowerXpressRequestHighPerformance = 1;
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int) {

  /**
   * App Module Composition
   */
  ovrly::js::registerHooks();
  ovrly::ui::registerHooks();
  ovrly::vr::registerHooks();

  /**
   * Get the CEF process initialized and executing ASAP
   */
  CefEnableHighDPISupport();

  // Use CEF to parse command-line arguments
  CefMainArgs main_args(hInstance);

  // Create app instance to handle CEF events
  CefRefPtr<CefApp> app(ovrly::process::Create());

  // Check if this is a child-process reentrant dispatch
  int exit_code = CefExecuteProcess(main_args, app, nullptr);
  if (exit_code >= 0) {
    // We were a child process which completed, gbye!
    return exit_code;
  }

  /*
   * Congratulations, we are the singleton browser process!
   */
  // Setup CEF initialization settings
  CefSettings settings;
  settings.no_sandbox = true;

  // Bind settings and app into CEF browser process init
  CefInitialize(main_args, settings, app, nullptr);

  // Pump the CEF message loop until CefQuitMessageLoop() is called
  CefRunMessageLoop();

  CefShutdown();

  return 0;
}
