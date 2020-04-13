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
static Event<Client&> OnClient;

/**
 * Create a new web client and get a reference to its `CefClient`
 */
CefRefPtr<CefClient> Create();

}} // namespaces
