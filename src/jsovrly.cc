#include "jsovrly.h"

#include <functional>
#include <include\wrapper\cef_message_router.h>

#include "appovrly.h"
#include "webovrly.h"

using namespace std::placeholders;


// Module exports
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
static CefRefPtr<CefMessageRouterRendererSide> rrouter_ = nullptr;

// When the render process is being created
void onRenderProcess(process::Render& rp) {
  // Create an instance of the render side message router
  rrouter_ = CefMessageRouterRendererSide::Create(getConfig());

  // Bind to the render process lifetime callbacks
  rp.SubOnContextCreated.attach(std::bind(&CefMessageRouterRendererSide::OnContextCreated, rrouter_, _1, _2, _3));
  rp.SubOnContextReleased.attach(std::bind(&CefMessageRouterRendererSide::OnContextReleased, rrouter_, _1, _2, _3));
  rp.SubOnProcessMessageReceived.attach(std::bind(&CefMessageRouterRendererSide::OnProcessMessageReceived, rrouter_, _1, _2, _3, _4));
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

static CefRefPtr<CefMessageRouterBrowserSide> brouter_ = nullptr;
static CefMessageRouterBrowserSide::Handler* bmsghandler = nullptr;

// When the browser process is being created
void onBrowserProcess(process::Browser& bp) {
  // create an instance of the browser side message router
  brouter_ = CefMessageRouterBrowserSide::Create(getConfig());

  // and handler
  bmsghandler = new MsgHandler();
  // Sub the handler to the router
  brouter_->AddHandler(bmsghandler, true);
  // TODO: Hook up the message handler
}

void onWebClient(web::Client& c) {
  // Hook the browser-side message router into the web client's CefApp handlers
  c.SubOnBeforeClose.attach(std::bind(&CefMessageRouterBrowserSide::OnBeforeClose, brouter_, _1));
  c.SubOnRenderProcessTerminated.attach([](auto browser, auto status) { brouter_->OnRenderProcessTerminated(browser); });
  c.SubOnBeforeBrowse.attach([](auto browser, auto frame, auto req, auto gesture, auto redirect) {
    brouter_->OnBeforeBrowse(browser, frame);
    return false;
  });
  c.SubOnProcessMessageReceived.attach(std::bind(&CefMessageRouterBrowserSide::OnProcessMessageReceived, brouter_, _1, _2, _3, _4));
}

} // module local


void registerHooks() {
  process::OnBrowser.attach(onBrowserProcess);
  process::OnRender.attach(onRenderProcess);
  web::OnClient.attach(onWebClient);
}
}} // module exports
