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
#include "jsovrly.h"
#include "mgrovrly.h"
#include "uiovrly.h"
#include "vrovrly.h"
#include "logging.h"

/**
 * Flag selection of high-performance gpu mode for NV and AMD hybrid setups
 */
extern "C"
{
	DLLEXPORT unsigned int NvOptimusEnablement = 0x00000001;
	DLLEXPORT int AmdPowerXpressRequestHighPerformance = 1;
}

#ifdef OS_WIN
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int) {
#else
int main(int argc, char *argv[]) {
#endif
  // Initialize the logging lib
  ovrly::logging::init();

  ovrly::logger::info("OVRLY STARTING");

  ovrly::logger::debug("(main) Composing");

  /* App Module Composition */
  ovrly::js::registerHooks();
  ovrly::vr::registerHooks();
  ovrly::ui::registerHooks();
  ovrly::mgr::registerHooks();

  ovrly::logger::debug("(main) Now well-composed");

  ovrly::logger::trace("OVRLY TRACE TEST!!");

  /* Get the CEF process initialized and executing ASAP */

  // Use CEF to parse command-line arguments
#ifdef OS_WIN
  CefEnableHighDPISupport();
  CefMainArgs main_args(hInstance);
#else
  CefMainArgs main_args(argc, argv);
#endif

  // Create app instance to handle CEF events
  CefRefPtr<CefApp> app(ovrly::process::Create());

  ovrly::logger::info("OVRLY starting webtech engine");

  // Check if this is a child-process reentrant dispatch
  int exit_code = CefExecuteProcess(main_args, app, nullptr);
  if (exit_code >= 0) {
    // We were a child process which completed, gbye!
    return exit_code;
  }

  ovrly::logger::debug("(main) Main Browser");

  /* Congratulations, we are the singleton browser process! */

  // Setup CEF initialization settings
  CefSettings settings;
  settings.remote_debugging_port = 8088;
  //settings.CefCommandLineArgs.Add("remote-allow-origins", "http://localhost:8080");
  CefString(&settings.root_cache_path) = "/home/josh/.cache/ovrly";
  settings.no_sandbox = true;
  settings.windowless_rendering_enabled = true;

  // Bind settings and app into CEF browser process init
  CefInitialize(main_args, settings, app, nullptr);

  // Pump the CEF message loop until CefQuitMessageLoop() is called
  CefRunMessageLoop();

  CefShutdown();

  return 0;
}
