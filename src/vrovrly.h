/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#pragma once

#include "events.h"

/**
 * The purpose of this module is to interact with the vr API to initialize it and
 * mediate interactions with it on behalf of other modules.
 *
 * It produces events for state changes (add/remove) and positions of tracked
 * peripherals and their buttons in the VR universe, virtual mouse inputs, and
 * other state changes to the vr system in general.
 */

namespace ovrly { namespace vr {

  extern Event<> OnReady;

  /**
   * Registers to launch the vr event thread once the browser process is initialized
   */
  void registerHooks();

}} // namespaces

