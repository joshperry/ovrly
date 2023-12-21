/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#include "imgovrly.h"
#include <openvr.h>

#include "logging.h"

namespace ovr = ::vr;
using namespace std::placeholders;

namespace ovrly{ namespace img{

// Module local
namespace {

  class ImageOverlay : public vr::Overlay {
    public:
      ImageOverlay(const std::string &name, mathfu::vec2 size, const std::string &path) :
        vr::Overlay(name, size)
      {
        // Set the initial size
        onLayout(size);

        // Render the image to the overlay
        renderImageFile(path);
      }

    protected:
      void onLayout(mathfu::vec2 size) override {
        // Calculate the pixel dimensions that the overlay will be rendered at
        // TODO: Calculate this based on optimum pixels/cm display in VR
        mathfu::vec2i psize(800 * size.y, 800);

        // Tell the overlay what pixel dimensions the browser will render to
        updateTargetSize(psize, {{1, 0}, {0, 1}});
      }
  };

 } // module local


 /*
 * Module exports
 */

std::unique_ptr<vr::Overlay> Create(const std::string &name, mathfu::vec2 size, std::string const &path) {
  logger::info("OVRLY Creating Image Overlay");

  return std::make_unique<ImageOverlay>(name, size, path);
}

}} // module exports
