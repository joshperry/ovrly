/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#include "webovrly.h"

#include <functional>

// Module exports
namespace ovrly{ namespace web{
  // Module local
  namespace {

    class ClientHandler: public Client,
                          public CefClient,
                          public CefLifeSpanHandler,
                          public CefRequestHandler
    {
      public:
        ClientHandler() {

        }

        void OnBeforeClose(CefRefPtr<CefBrowser> browser) override
        {
          SubOnBeforeClose(browser);
        }

        void OnRenderProcessTerminated(CefRefPtr< CefBrowser > browser, CefRequestHandler::TerminationStatus status) override
        {
          SubOnRenderProcessTerminated(browser, status);
        }

        bool OnBeforeBrowse(CefRefPtr<CefBrowser> browser,
          CefRefPtr<CefFrame> frame,
          CefRefPtr<CefRequest> request,
          bool user_gesture,
          bool is_redirect) override
        {
          return SubOnBeforeBrowse(browser, frame, request, user_gesture, is_redirect);
        }

        bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
          CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override
        {
          return SubOnProcessMessageReceived(browser, frame, source_process, message);
        }

      private:
        IMPLEMENT_REFCOUNTING(ClientHandler);
    };

    class WebClient : public CefClient {
      public:
        WebClient(CefRefPtr<ClientHandler> handler) : handler_(handler) {
          
        }

        CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override {
          return handler_;
        }

        CefRefPtr<CefRequestHandler> GetRequestHandler() override {
          return handler_;
        }

        bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
          CefRefPtr<CefFrame> frame,
          CefProcessId source_process,
          CefRefPtr<CefProcessMessage> message) override
        {
          return handler_->OnProcessMessageReceived(browser, frame, source_process, message);
        }

      private:
        CefRefPtr<ClientHandler> handler_;

        IMPLEMENT_REFCOUNTING(WebClient);
    };
  } // module local

  
  Event<Client&> OnClient;

  CefRefPtr<CefClient> Create() {
    CefRefPtr<ClientHandler> handler = new ClientHandler();

    // Notify observers
    OnClient(*handler);

    return new WebClient(handler);
  }

}} // module exports
