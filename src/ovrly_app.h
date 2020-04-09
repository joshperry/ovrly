/* 
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 * 
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by  
 * the Free Software Foundation, version 3.
 */
#pragma once

#include "include/cef_app.h"

// Implement application-level callbacks for the browser process.
class OvrlyApp : public CefApp, public CefBrowserProcessHandler {
 public:
  OvrlyApp();

  // CefApp impl
  virtual CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() OVERRIDE {
    return this;
  }

  // CefBrowserProcessHandler impl
  virtual void OnContextInitialized() OVERRIDE;

 private:
  IMPLEMENT_REFCOUNTING(OvrlyApp);
};
