/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#include "uiovrly.h"

#include <sstream>
#include <string>
#include "include/base/cef_callback.h"
#include "include/cef_app.h"
#include "include/cef_parser.h"
#include "include/views/cef_browser_view.h"
#include "include/views/cef_window.h"
#include "include/wrapper/cef_closure_task.h"
#include "include/wrapper/cef_helpers.h"

#include "appovrly.h"
#include "webovrly.h"


namespace ovrly{ namespace ui{

// Module local
namespace {

  // Returns a data: URI with the specified contents.
  std::string GetDataURI(const std::string& data, const std::string& mime_type) {
    return "data:" + mime_type + ";base64," +
           CefURIEncode(CefBase64Encode(data.data(), data.size()), false)
               .ToString();
  }

  /**
   * This cefclient impl is meant for the visible browser instance
   * that hosts the ovrly UI
   */
  class uiclient : public web::Client,
                      public CefClient,
                      public CefDisplayHandler,
                      public CefLifeSpanHandler,
                      public CefLoadHandler,
                      public CefRequestHandler
  {
   public:
    explicit uiclient(bool use_views)
      : use_views_(use_views), is_closing_(false)
    {
    }

    // CefClient methods:
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
    CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }

    // CefDisplayHandler methods:
    void OnTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title) override {
      CEF_REQUIRE_UI_THREAD();

      if (use_views_) {
        // Set the title of the window using the Views framework.
        CefRefPtr<CefBrowserView> browser_view =
          CefBrowserView::GetForBrowser(browser);
        if (browser_view) {
          CefRefPtr<CefWindow> window = browser_view->GetWindow();
          if (window)
            window->SetTitle(title);
        }
      }
      else {
        // Set the title of the window using platform APIs.
        // FIXME(linux): For Linux support this needs an impl
        //PlatformTitleChange(browser, title);
      }
    }

    // CefLifeSpanHandler methods:
    void OnAfterCreated(CefRefPtr<CefBrowser> browser) override {
      CEF_REQUIRE_UI_THREAD();

      // Add to the list of existing browsers.
      browser_list_.push_back(browser);
    }

    bool DoClose(CefRefPtr<CefBrowser> browser) override {
      CEF_REQUIRE_UI_THREAD();

      // Closing the main window requires special handling. See the DoClose()
      // documentation in the CEF header for a detailed destription of this
      // process.
      if (browser_list_.size() == 1) {
        // Set a flag to indicate that the window close should be allowed.
        is_closing_ = true;
      }

      // Allow the close. For windowed browsers this will result in the OS close
      // event being sent.
      return false;
    }

    void OnBeforeClose(CefRefPtr<CefBrowser> browser) override {
      CEF_REQUIRE_UI_THREAD();

      // Notify observers
      SubOnBeforeClose(browser);

      // Remove from the list of existing browsers.
      BrowserList::iterator bit = browser_list_.begin();
      for (; bit != browser_list_.end(); ++bit) {
        if ((*bit)->IsSame(browser)) {
          browser_list_.erase(bit);
          break;
        }
      }

      if (browser_list_.empty()) {
        // All browser windows have closed. Quit the application message loop.
        CefQuitMessageLoop();
      }
    }

    // CefLoadHandler methods:
    void OnLoadError(CefRefPtr<CefBrowser> browser,
                      CefRefPtr<CefFrame> frame,
                      ErrorCode errorCode,
                      const CefString& errorText,
                      const CefString& failedUrl) override
    {
      CEF_REQUIRE_UI_THREAD();

      // Don't display an error for downloaded files.
      if (errorCode == ERR_ABORTED)
        return;

      // Display a load error message using a data: URI.
      std::stringstream ss;
      ss << "<html><body bgcolor=\"white\">"
        "<h2>Failed to load URL "
        << std::string(failedUrl) << " with error " << std::string(errorText)
        << " (" << errorCode << ").</h2></body></html>";

      frame->LoadURL(GetDataURI(ss.str(), "text/html"));
    }

    // Request that all existing browser windows close.
    void CloseAllBrowsers(bool force_close) {
      if (!CefCurrentlyOn(TID_UI)) {
        // Execute on the UI thread.
        CefPostTask(TID_UI, CefCreateClosureTask(base::BindOnce(&uiclient::CloseAllBrowsers, this,
                                       force_close)));
        return;
      }

      if (browser_list_.empty())
        return;

      BrowserList::const_iterator it = browser_list_.begin();
      for (; it != browser_list_.end(); ++it)
        (*it)->GetHost()->CloseBrowser(force_close);
    }

    bool IsClosing() const { return is_closing_; }

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
    // Platform-specific implementation.
    //void PlatformTitleChange(CefRefPtr<CefBrowser> browser, const CefString& title);

    // True if the application is using the Views framework.
    const bool use_views_;

    // List of existing browser windows. Only accessed on the CEF UI thread.
    typedef std::list<CefRefPtr<CefBrowser>> BrowserList;
    BrowserList browser_list_;

    bool is_closing_;

    // Include the default reference counting implementation.
    IMPLEMENT_REFCOUNTING(uiclient);
  };

  // When using the Views framework this object provides the delegate
  // implementation for the CefWindow that hosts the Views-based browser.
  class OvrlyWindowDelegate : public CefWindowDelegate {
  public:
    explicit OvrlyWindowDelegate(CefRefPtr<CefBrowserView> browser_view)
      : browser_view_(browser_view) {}

    void OnWindowCreated(CefRefPtr<CefWindow> window) override {
      // Add the browser view and show the window.
      window->AddChildView(browser_view_);
      window->Show();

      // Give keyboard focus to the browser view.
      browser_view_->RequestFocus();
    }

    void OnWindowDestroyed(CefRefPtr<CefWindow> window) override {
      browser_view_ = nullptr;
    }

    bool CanClose(CefRefPtr<CefWindow> window) override {
      // Allow the window to close if the browser says it's OK.
      CefRefPtr<CefBrowser> browser = browser_view_->GetBrowser();
      if (browser)
        return browser->GetHost()->TryCloseBrowser();
      return true;
    }

    CefSize GetPreferredSize(CefRefPtr<CefView> view) override {
      return CefSize(800, 600);
    }

  private:
    CefRefPtr<CefBrowserView> browser_view_;

    IMPLEMENT_REFCOUNTING(OvrlyWindowDelegate);
    DISALLOW_COPY_AND_ASSIGN(OvrlyWindowDelegate);
  };

  class BrowserViewDelegate : public CefBrowserViewDelegate {
  public:
    BrowserViewDelegate() {}

    bool OnPopupBrowserViewCreated(CefRefPtr<CefBrowserView> browser_view,
      CefRefPtr<CefBrowserView> popup_browser_view,
      bool is_devtools) override {
      // Create a new top-level Window for the popup. It will show itself after
      // creation.
      CefWindow::CreateTopLevelWindow(
        new OvrlyWindowDelegate(popup_browser_view));

      // We created the Window.
      return true;
    }

  private:
    IMPLEMENT_REFCOUNTING(BrowserViewDelegate);
    DISALLOW_COPY_AND_ASSIGN(BrowserViewDelegate);
  };


  /**
   * Launch the ovrly UI browser window
   */
  void launchui() {
    CEF_REQUIRE_UI_THREAD();

    // Get the main process commandline options
    CefRefPtr<CefCommandLine> command_line = CefCommandLine::GetGlobalCommandLine();

#if defined(OS_WIN) || defined(OS_LINUX)
    // Create the browser using the Views framework if "--use-views" is specified
    // via the command-line. Otherwise, create the browser using the native
    // platform framework. The Views framework is currently only supported on
    // Windows and Linux.
    const bool useviews = true;
#else
    const bool useviews = false;
#endif

    // UI browser settings
    CefBrowserSettings browser_settings;

    // Check if a "--url=" value was provided via the command-line. If so, use
    // that instead of the default URL.
    std::string url = command_line->GetSwitchValue("url");
    if (url.empty())
      url = "https://curiouslynerdy.com/ovrly";

    // Create the UI web client and notify listeners of its creation
    CefRefPtr<uiclient> client(new uiclient(useviews));
    OnClient(*client);

    if (useviews) {
      // Create the BrowserView.
      CefRefPtr<CefBrowserView> browser_view = CefBrowserView::CreateBrowserView(
          client, url, browser_settings, nullptr, nullptr,
          new BrowserViewDelegate()
      );

      // Create the Window. It will show itself after creation.
      CefWindow::CreateTopLevelWindow(new OvrlyWindowDelegate(browser_view));
    } else {
      // Information used when creating the native window.
      CefWindowInfo window_info;

#if defined(OS_WIN)
      // On Windows we need to specify certain flags that will be passed to
      // CreateWindowEx().
      window_info.SetAsPopup(NULL, "ovrly");
#endif

      // Create the first browser window.
      CefBrowserHost::CreateBrowser(window_info, client, url, browser_settings, nullptr, nullptr);
    }
  }

  void onBrowserProcess(process::Browser &browser) {
    /*
     * Launch the ovrly UI browser and run app startup logic
     * once the CEF browser UI thread context is up and running
     */
    browser.SubOnContextInitialized.attach([]() {
      launchui();
    });
  }

}  // module local


/*
 * Module exports
 */

Event<web::Client&> OnClient;

void registerHooks() {
  process::OnBrowser.attach(onBrowserProcess);
}

}} // module exports
