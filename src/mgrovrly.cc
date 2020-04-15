/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#include "appovrly.h"

#include "vrovrly.h"
#include "webovrly.h"

namespace ovrly{ namespace mgr{

// Module local
namespace {

  bool cefready = false;
  bool vrready = false;

  void onBrowserProcess(process::Browser &browser) {
    /*
     * Launch the overlays once the UI is up and running
     */
    browser.SubOnContextInitialized.attach([]() {
      cefready = true;
      if(vrready) {
        web::Create();
      }
    });
  }

  void onVRReady() {
    vrready = true;
    if(cefready) {
      web::Create();
    }
  }

}  // module local


/*
 * Module exports
 */

void registerHooks() {
  process::OnBrowser.attach(onBrowserProcess);
  vr::OnReady.attach(onVRReady);
}

}} // module exports

