/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#pragma once

#include "include/cef_client.h"

#include <list>

#include "webovrly.h"

namespace ovrly{ namespace ui{
  /**
   * Observable for notification when the UI web client is created
   *
   * Guaranteed to be dispatched before any client events are raised
   */
  extern Event<web::Client&> OnClient;

  /**
   * Registers to launch the ovrly UI once the browser process is initialized
   */
  void registerHooks();
}} // namespaces
