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
#include "imgovrly.h"

namespace ovrly{ namespace mgr{

// Module local
namespace {

  std::vector<std::unique_ptr<vr::Overlay>> overlays_;

  void onVRReady() {
    // Browser overlay
    auto overlay = web::Create(
      "FishTank",
      // hieght meters, aspect
      mathfu::vec2(1.5f, 1.33333333f),
      "https://webglsamples.org/aquarium/aquarium.html"
    );

    auto xform = overlay->getTransform();
    // right, up, away meters
    overlay->setTransform(xform * mathfu::mat4::FromTranslationVector({0.0f, 1.0f, -2.0f}));

    overlays_.push_back(std::move(overlay));

    // Static image overlay
    auto imgoverlay = img::Create(
      "mypic",
      mathfu::vec2(1.0f, 1.33333333f),
      "/home/josh/Downloads/drzzl2.png"
    );

    xform = imgoverlay->getTransform();
    imgoverlay->setTransform(xform * mathfu::mat4::FromTranslationVector({0.0f, 2.125f, -1.65f}) * mathfu::mat4::FromRotationMatrix(mathfu::mat4::RotationX(45*(M_PI/180))));

    overlays_.push_back(std::move(imgoverlay));
  }

}  // module local

/*
 * Module exports
 */

void registerHooks() {
  vr::OnReady.attach(onVRReady);
}

}} // module exports
