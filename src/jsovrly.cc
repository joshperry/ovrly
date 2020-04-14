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
    // Create an instance of the render side message router
    rrouter_ = CefMessageRouterRendererSide::Create(getConfig());

    // Bind the message router to the render process lifetime callbacks
    rp.SubOnContextCreated.attach(std::bind(&CefMessageRouterRendererSide::OnContextCreated, rrouter_, _1, _2, _3));
    rp.SubOnContextReleased.attach(std::bind(&CefMessageRouterRendererSide::OnContextReleased, rrouter_, _1, _2, _3));
    rp.SubOnProcessMessageReceived.attach(std::bind(&CefMessageRouterRendererSide::OnProcessMessageReceived, rrouter_, _1, _2, _3, _4));

    // TODO: Inject javascript API
  }


  /**
   * Native side of the native-to-js message router
   */
  class MsgHandler : public CefMessageRouterBrowserSide::Handler {
    public:
      typedef std::function<bool(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>,
          int64, const CefString&, bool, CefRefPtr<Callback>)> OnQuery_t;
      typedef std::function<void(CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, int64)> OnCancel_t;

      MsgHandler() {}

      FilterChain< CefRefPtr<CefBrowser>,
        CefRefPtr<CefFrame>,
        int64,
        const CefString&,
        bool,
        CefRefPtr<Callback> > SubOnQuery;
      bool OnQuery(CefRefPtr<CefBrowser> browser,
                   CefRefPtr<CefFrame> frame,
                   int64 query_id,
                   const CefString& request,
                   bool persistent,
                   CefRefPtr<Callback> callback) override
      {
        return SubOnQuery(browser, frame, query_id, request, persistent, callback);
      }

      Event< CefRefPtr<CefBrowser>, CefRefPtr<CefFrame>, int64 > SubOnCancel;
      void OnQueryCanceled(CefRefPtr<CefBrowser> browser,
                           CefRefPtr<CefFrame> frame,
                           int64 query_id) override
      {
        SubOnCancel(browser, frame, query_id);
      }
  };

  CefRefPtr<CefMessageRouterBrowserSide> brouter_;
  CefMessageRouterBrowserSide::Handler* bmsghandler;

  // When the browser process is being created
  void onBrowserProcess(process::Browser& browser) {
    // create an instance of the browser side message router
    brouter_ = CefMessageRouterBrowserSide::Create(getConfig());

    // Set up the handler when the context is done setting up
    // since this needs to be done on the UI thread
    browser.SubOnContextInitialized.attach([]() {
      bmsghandler = new MsgHandler();
      brouter_->AddHandler(bmsghandler, true);
      // TODO: Hook up the message handler
    });
  }

  // When a web client is created
  void onWebClient(web::Client& c) {
    // Hook the browser-side message router into the web client's CefApp handlers
    c.SubOnBeforeClose.attach(std::bind(&CefMessageRouterBrowserSide::OnBeforeClose, brouter_, _1));
    c.SubOnRenderProcessTerminated.attach([](auto browser, auto status) { brouter_->OnRenderProcessTerminated(browser); });
    c.SubOnBeforeBrowse.attach([](auto browser, auto frame, auto req, auto gesture, auto redirect) {
      brouter_->OnBeforeBrowse(browser, frame);
      // Always return false so the browse op continues normally,
      // just need to hook this for router lifecycle handling
      return false;
    });
    c.SubOnProcessMessageReceived.attach(std::bind(&CefMessageRouterBrowserSide::OnProcessMessageReceived, brouter_, _1, _2, _3, _4));
  }

} // module local


/*
 * Module exports
 */

void registerHooks() {
  process::OnBrowser.attach(onBrowserProcess);
  process::OnRender.attach(onRenderProcess);
  web::OnClient.attach(onWebClient);
  ui::OnClient.attach(onWebClient);
}

}} // module exports
