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

#include "events.h"

/**
 * The purpose of this module is to manage the browser and rendering path
 * for web-based overlays by taking textures from the browser and committing
 * them to the vr overlay API for compositing.
 *
 * It detects changes made to the size and aspect ratio of the overlay and
 * reconfigures the browser instance to produce the correct textures for new metrics.
 *
 * It also handles hooks virtual mouse inputs from the vr API and injects them into
 * the browser instance to support interacting with the overlay.
 */

namespace ovrly{ namespace web{

/**
 * Observables for hooking lifetime events on handlers provided by a
 * web client's `CefClient`.
 */
class Client {
  public:
    Event< CefRefPtr<CefBrowser> > SubOnBeforeClose;

    Event< CefRefPtr<CefBrowser>, CefRequestHandler::TerminationStatus > SubOnRenderProcessTerminated;

    FilterChain< CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefRefPtr<CefRequest>, bool, bool >
    SubOnBeforeBrowse;

    FilterChain< CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, CefProcessId, CefRefPtr<CefProcessMessage> >
    SubOnProcessMessageReceived;
};

/**
 * Observable for notification when a new web client is created
 *
 * Guaranteed to be dispatched before any client events are raised
 */
extern Event<Client&> OnClient;

/**
 * Create a new web client and get a reference to its `CefClient`
 */
CefRefPtr<CefClient> Create();

}} // namespaces
