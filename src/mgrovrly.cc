/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#include "appovrly.h"

#include "ovrly.h"
#include "vrovrly.h"
#include "webovrly.h"

namespace ovrly{ namespace mgr{

// Module local
namespace {

  std::vector<std::unique_ptr<vr::Overlay>> overlays_;

  void onVRReady() {
    overlays_.push_back(web::Create(
        mathfu::vec2(1.5f, 1.33333333f),
        "https://webglsamples.org/aquarium/aquarium.html"
    ));
  }

}  // module local


/*
 * Module exports
 */

void registerHooks() {
  vr::OnReady.attach(onVRReady);
}

}} // module exports
