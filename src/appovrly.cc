/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#include "appovrly.h"

namespace ovrly { namespace process {

// Module locals
namespace {

/**
 * BrowserProcessHandler
 */
class BrowserProcessHandler : public Browser, public CefBrowserProcessHandler {
  public:
    BrowserProcessHandler() { }

    void OnContextInitialized() override
    {
      SubOnContextInitialized();
    }

  private:
    IMPLEMENT_REFCOUNTING(BrowserProcessHandler);
    DISALLOW_COPY_AND_ASSIGN(BrowserProcessHandler);
};


/**
 * RenderProcessHandler
 */
class RenderProcessHandler : public Render, public CefRenderProcessHandler {
  public:
    RenderProcessHandler() {
    }

    void OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
      CefRefPtr<CefV8Context> context) override
    {
      SubOnContextCreated(browser, frame, context);
    }

    void OnContextReleased(CefRefPtr< CefBrowser > browser, CefRefPtr< CefFrame > frame,
      CefRefPtr< CefV8Context > context) override
    {
      SubOnContextReleased(browser, frame, context);
    }

    bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
      CefProcessId source_process, CefRefPtr<CefProcessMessage> message) override
    {
      return SubOnProcessMessageReceived(browser, frame, source_process, message);
    }

  private:
    IMPLEMENT_REFCOUNTING(RenderProcessHandler);
    DISALLOW_COPY_AND_ASSIGN(RenderProcessHandler);
};

bool isbrowser{ true };

// Implement application-level callbacks for the browser processes.
class App : public CefApp {
  public:
    CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override {
      if(!browser_) {
        browser_ = new BrowserProcessHandler();
        // Notify observers
        OnBrowser(*browser_);
      }

      return browser_;
    }

    // These handlers are executed in the render process
    CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override {
      isbrowser = false;
      if(!render_) {
        render_ = new RenderProcessHandler();
        // Notify observers
        OnRender(*render_);
      }

      return render_;
    }

  private:
    CefRefPtr<BrowserProcessHandler> browser_;
    CefRefPtr<RenderProcessHandler> render_;

    IMPLEMENT_REFCOUNTING(App);
};

// Allows wrapping a std::function in a CefTask
class FuncTask : public CefTask {
  public:
    FuncTask(std::function<void()> &&func) : func_(std::move(func)) { }

    void Execute() override {
      func_();
    }

  private:
    std::function<void()> func_;

    IMPLEMENT_REFCOUNTING(FuncTask);
    DISALLOW_COPY_AND_ASSIGN(FuncTask);
};

}  // module local


/*
 * Module exports
 */

Event<Browser&> OnBrowser;
Event<Render&> OnRender;

CefRefPtr<CefApp> Create() {
  return new App();
}

void runOnMain(std::function<void()> &&func) {
  CefPostTask(isbrowser ? cef_thread_id_t::TID_UI : cef_thread_id_t::TID_RENDERER, new FuncTask(std::move(func)));
}

}} // module exports
