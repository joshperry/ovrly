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
 * The purpose of this module is to define the native-to-js API and
 * the logic to do cross-process pub-sub to dispatch events from native code
 * running in the browser process to js code running in any number of v8 contexts.
 *
 * ZMQ is used both for publishing changes in the native state to the v8
 * contexts in the render processes, and for RPC from the the render processes
 * to mutate state managed by the browser process.
 *
 * The vr event loop runs in its own thread, but events are always dispatched
 * on the main UI thread.
 */

namespace ovrly{ namespace js{
  /**
   * Register javascript composition hooks
   */
  void registerHooks();
}}
