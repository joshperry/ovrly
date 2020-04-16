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

#include "events.h"

/**
 * This module is in charge of mediating the chromium browser and render
 * processes startup and producing notifications to other modules so that they can
 * run as needed for the respective process type.
 */

namespace ovrly{ namespace process{

/**
 * Observables for hooking lifetime events on CefBrowserProcessHandler
 */
class Browser {
  public:
    Event<> SubOnContextInitialized;
};

/**
 * Observables for hooking lifetime events on CefRenderProcessHandler
 */
class Render {
  public:
    Event< CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefV8Context> >
    SubOnContextCreated;

    Event< CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefV8Context> >
    SubOnContextReleased;

    FilterChain< CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefProcessId, CefRefPtr<CefProcessMessage> >
    SubOnProcessMessageReceived;
};

/**
 * Observable for notification when this is a browser process
 *
 * Guaranteed to be dispatched before any browser process events are raised
 */
extern Event<Browser&> OnBrowser;
/**
 * Observable for notification when this is a render process
 *
 * Guaranteed to be dispatched before any render process events are raised
 */
extern Event<Render&> OnRender;

/**
 * Create a new app and get an instance to its CefApp
 */
CefRefPtr<CefApp> Create();

}} // namespace
