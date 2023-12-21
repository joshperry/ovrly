/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#pragma once

#include "vrovrly.h"

/**
 * The purpose of this module is to implement a static image-based overlay
 */

namespace ovrly{ namespace img{

/**
 * Create a new img Overlay
 */
std::unique_ptr<vr::Overlay> Create(const std::string &name, mathfu::vec2 size, std::string const &path);

}} // namespaces
