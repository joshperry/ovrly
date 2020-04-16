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
 * The purpose of this module is to handle the native-side aspects of
 * managing overlays.
 *
 * It implements logic like detecing vr preripherals intersecting with overlays
 * and state changes in the overlay interactions for selecting, organizing, and
 * positioning.
 *
 * Only logic which is latency sensitive for comfortable interaction needs to be
 * implemented here, any logic which can handle the latency in a transition into
 * the javascript runtime should be implemented in the js layer.
 */

namespace ovrly{ namespace mgr{
  /**
   * Registers to launch the ovrly UI once the browser process is initialized
   */
  void registerHooks();
}} // namespaces
