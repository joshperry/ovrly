/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#pragma once

/**
 * The purpose of this module is to define the native-to-js interface and
 * the logic to do cross-process pub-sub to dispatch events from native code
 * running in the browser process to js code running in any number of v8 contexts.
 *
 * This is a singleton module since only one is created per process.
 *
 * Provides hook functions to call from `CefRenderProcessHandler` functions that
 * injects native-to-js objects in into each v8 context and sets up and owns the
 * CefMessageRouterRendererSide of the [message router]
 * (https://bitbucket.org/chromiumembedded/cef/src/master/include/wrapper/cef_message_router.h).
 *
 * Provides hook functions to call from `CefBrowserProcessHandler functions that
 * sets up and owns the browser process side message router.
 *
 * The browser side of the router is defined in the `CefBrowserProcessHandler`.
 */
namespace ovrly{ namespace js{
  /**
   * Register javascript composition hooks
   */
  void registerHooks();
}}