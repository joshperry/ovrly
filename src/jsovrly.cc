#include "jsovrly.h"

#include <functional>
#include <include\wrapper\cef_message_router.h>

#include "appovrly.h"
#include "webovrly.h"
#include "uiovrly.h"

using namespace std::placeholders;


namespace ovrly { namespace js {

// Module local
namespace {

  // Configure the message routers
  CefMessageRouterConfig getConfig() {
    CefMessageRouterConfig config;
    config.js_query_function = "ovrlyNativeQuery";
    config.js_cancel_function = "ovrlyNativeQueryCancel";
    return config;
  }


  /**
   * Message router for the render process
   */
  CefRefPtr<CefMessageRouterRendererSide> rrouter_ = nullptr;

  // When the render process is being created, hook the message router lifecycle and
  // inject the js API interface
  //
  // The js setup is currently identical for both the UI client and the overlay clients,
  // but future versions may want to differentiate the js interface between the two.
  void onRenderProcess(process::Render& rp) {
    // Create an instance of the render-side message router
    rrouter_ = CefMessageRouterRendererSide::Create(getConfig());

    // Bind the message router to the render process lifetime callbacks
    rp.SubOnContextCreated.attach([](auto browser, auto frame, auto context) {
      rrouter_->OnContextCreated(browser, frame, context);

      // TODO: Inject javascript API
    });

    rp.SubOnContextReleased.attach([](auto browser, auto frame, auto context) {
      rrouter_->OnContextReleased(browser, frame, context);
    });

    rp.SubOnProcessMessageReceived.attach([](auto browser, auto frame, auto source_process, auto message) {
      return rrouter_->OnProcessMessageReceived(browser, frame, source_process, message);
    });
  }


  /**
   * Native browser side of the native-to-js message router
   */
  class MsgHandler : public CefMessageRouterBrowserSide::Handler {
    public:
      bool OnQuery(CefRefPtr<CefBrowser> browser,
        CefRefPtr<CefFrame> frame,
        int64 query_id,
        const CefString& request,
        bool persistent,
        CefRefPtr<Callback> callback) override
      {
        return false;
      }

      Event< CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, int64 > SubOnCancel;
      void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           int64 query_id) override
      {

      }
  };

  CefRefPtr<CefMessageRouterBrowserSide> brouter_;
  MsgHandler* bmsghandler;

  // When the browser process is being created
  void onBrowserProcess(process::Browser& browser) {
    // create an instance of the browser-side message router
    brouter_ = CefMessageRouterBrowserSide::Create(getConfig());

    // Set up the handler when the context is done setting up
    // since this needs to be done on the UI thread
    browser.SubOnContextInitialized.attach([]() {
      bmsghandler = new MsgHandler();
      brouter_->AddHandler(bmsghandler, true);
    });
  }

  // When a web client is created
  void onWebClient(web::Client& c) {
    // Hook the browser-side message router into the web client's CefApp handlers
    c.SubOnBeforeClose.attach([](auto browser) {
      brouter_->OnBeforeClose(browser);
    });

    c.SubOnRenderProcessTerminated.attach([](auto browser, auto status) {
      brouter_->OnRenderProcessTerminated(browser);
    });

    c.SubOnBeforeBrowse.attach([](auto browser, auto frame, auto req, auto gesture, auto redirect) {
      brouter_->OnBeforeBrowse(browser, frame);
      // Always return false so the browse op continues normally,
      // just need to hook this for router lifecycle handling
      return false;
    });

    c.SubOnProcessMessageReceived.attach([](auto browser, auto frame, auto source_process, auto message) {
      return brouter_->OnProcessMessageReceived(browser, frame, source_process, message);
    });
  }

} // module local


/*
 * Module exports
 */

void registerHooks() {
  process::OnBrowser.attach(onBrowserProcess);
  process::OnRender.attach(onRenderProcess);

  // Hook notifications to get the `CefClient` of new browsers to inject js
  ui::OnClient.attach(onWebClient);
  web::OnClient.attach(onWebClient);
}

}} // module exports
