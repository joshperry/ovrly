#include "jsovrly.h"

#include <string>
#include <functional>
#include <thread>
#include "zmq.hpp"

#include "appovrly.h"
#include "webovrly.h"
#include "uiovrly.h"
#include "logging.h"
#include "serralize.hpp"

using namespace std::string_literals;

namespace ovrly { namespace js {

// Module local
namespace {

  // ZMQ context for browser _and_ render process zmq connections
  const char* ZMQ_URL = "tcp://127.0.0.1:9995";
  zmq::context_t zctx_{ 1 };
  std::unique_ptr<zmq::socket_t> zsock_;

  // Thread for running the zmq receive loop
  std::unique_ptr<std::thread> zloop_;
  std::atomic<bool> done{ false };

  // When the browser process is being created
  void onBrowserProcess(process::Browser& browser) {
    browser.SubOnContextInitialized.attach([]() {
      // Setup and bind the zmq publish socket
      zsock_ = std::make_unique<zmq::socket_t>(zctx_, zmq::socket_type::pub);
      try {
        zsock_->bind(ZMQ_URL);
      } catch(zmq::error_t& err) {
        logger::error("Problem binding zmq listener: {}", err.what());
      }
    });
  }

  // When the render process is being created
  void onRenderProcess(process::Render& rp) {
    logger::debug("Render process spawned!");
    // Setup the zmq IPC connection and inject the js API interface.
    rp.SubOnContextCreated.attach([](CefRefPtr<CefBrowser> browser, auto frame, CefRefPtr<CefV8Context> jsctx) {
      // TODO: Setup the js API
      // The js setup is currently identical for both the UI client and the overlie clients,
      // but future versions may want to differentiate the js interface between the two.
      logger::debug("Render process spawned JS context!");

      auto ovrly = CefV8Value::CreateObject(nullptr, nullptr);
      ovrly->SetValue(L"initialized", CefV8Value::CreateBool(true), CefV8Value::PropertyAttribute::V8_PROPERTY_ATTRIBUTE_READONLY);
      jsctx->GetGlobal()->SetValue(L"ovrly", ovrly, CefV8Value::PropertyAttribute::V8_PROPERTY_ATTRIBUTE_READONLY);

      // Start a thread to receive zmq IPC messages
      zloop_ = std::make_unique<std::thread>([jsctx]() {
        logger::debug("Spawned ZMQ message loop thread");

        try {
          while(1) {
            // Setup and connect the zmq sub socket
            zsock_ = std::make_unique<zmq::socket_t>(zctx_, zmq::socket_type::sub);
            zsock_->connect(ZMQ_URL);

            zsock_->set(zmq::sockopt::subscribe, "vr.devices");

            // See if there is a message waiting
            auto msg = std::make_shared<zmq::message_t>();
            auto result = zsock_->recv(*msg);
            if(result) {
              // Got the envelope topic
              auto topic = msg->to_string();

              // Get the envelope body
              result = zsock_->recv(*msg);
              if(result) {
                // Update the js data on its thread
                process::runOnMain([jsctx, topic, msg = std::move(msg)]() {
                  // TODO: Process zmq message into js

                  // Deserialize the message data
                  std::vector<vr::TrackedDevice> devices;
                  serralize::InMemStream instream(msg->data(), msg->size());
                  deserialize(instream, &devices);

                  jsctx->Enter();

                  auto ovrly = jsctx->GetGlobal()->GetValue(L"ovrly");
                  if(!ovrly->HasValue(L"devices")) {
                    auto hmdev = CefV8Value::CreateObject(nullptr, nullptr);
                    hmdev->SetValue(L"manufacturer", CefV8Value::CreateString(devices[0].manufacturer), CefV8Value::PropertyAttribute::V8_PROPERTY_ATTRIBUTE_READONLY);
                    hmdev->SetValue(L"model", CefV8Value::CreateString(devices[0].model), CefV8Value::PropertyAttribute::V8_PROPERTY_ATTRIBUTE_READONLY);
                    hmdev->SetValue(L"serial", CefV8Value::CreateString(devices[0].serial), CefV8Value::PropertyAttribute::V8_PROPERTY_ATTRIBUTE_READONLY);

                    // ovrly.devices.hmd
                    auto jsdevs = CefV8Value::CreateObject(nullptr, nullptr);
                    jsdevs->SetValue(L"hmd", hmdev, CefV8Value::PropertyAttribute::V8_PROPERTY_ATTRIBUTE_READONLY);

                    // ovrly.devices
                    ovrly->SetValue(L"devices", jsdevs, CefV8Value::PropertyAttribute::V8_PROPERTY_ATTRIBUTE_READONLY);
                  }

                  auto jsdevs = ovrly->GetValue(L"devices");
                  auto hmdev = jsdevs->GetValue(L"hmd");

                  hmdev->SetValue(L"connected", CefV8Value::CreateBool(devices[0].connected), CefV8Value::PropertyAttribute::V8_PROPERTY_ATTRIBUTE_READONLY);

                  if(devices[0].pose) {
                    if(!hmdev->HasValue(L"pose")) {
                      // ovrly.devices.hmd.pose.matrix
                      auto hmdevmat = CefV8Value::CreateArray(12);
                      auto hmdevpose = CefV8Value::CreateObject(nullptr, nullptr);
                      hmdevpose->SetValue(L"matrix", hmdevmat, CefV8Value::PropertyAttribute::V8_PROPERTY_ATTRIBUTE_READONLY);
                      hmdev->SetValue(L"pose", hmdevpose, CefV8Value::PropertyAttribute::V8_PROPERTY_ATTRIBUTE_READONLY);
                    }

                    auto hmdevpose = hmdev->GetValue(L"pose");
                    auto hmdevmat = hmdevpose->GetValue(L"matrix");

                    for(int i = 0; i < 12; i++) {
                      hmdevmat->SetValue(i, CefV8Value::CreateDouble(devices[0].pose.value().matrix[i]));
                    }
                  }

                  jsctx->Exit();
                });
              }
            }
          }
        } catch(zmq::error_t &err) {
          if(err.num() != ETERM) {
            logger::error("Error receiving zmq message: {}", err.what());
          }
          zsock_->close();
        }
      });
    });

    rp.SubOnContextReleased.attach([](auto browser, auto frame, auto jsctx) {
      // Signal the zmq message loop thread to terminate
      zctx_.close(); // blocks until sockets are closed
      zloop_.reset();
    });
  }

  // When a web client is created
  void onWebClient(web::Client& c) {

  }

  // Send an updated device state list to the each render process
  void sendDevices(std::shared_ptr<const std::vector<vr::TrackedDevice>> devices) {
    // TODO: optimize to skip sending unchanged payloads across the native->js barrier

    try {
      // Serialize the devices
      auto outstream = new serralize::OutMemStream();
      serralize::serialize(*outstream, *devices.get());

      zmq::message_t msg(
        &outstream->getBuf()[0],
        outstream->getBuf().size(),
        [](void* buf, void* sharedref) { delete static_cast<serralize::OutMemStream*>(sharedref); },
        outstream
      );

      // Send the envelope topic part
      zsock_->send(zmq::message_t("vr.devices.updated"s), zmq::send_flags::sndmore);

      // Send the body part
      zsock_->send(std::move(msg));
    } catch(zmq::error_t& err) {
      logger::error("Sending zmq message failed: {}", err.what());
    }
  }

  // When the VR module is ready to go
  void onVRReady() {
    auto devices = std::make_shared<std::vector<vr::TrackedDevice>>(vr::getDevices());
    sendDevices(devices);
  }

  // When we receive a device state update from the VR module
  void onDevicesUpdated(std::shared_ptr<const std::vector<vr::TrackedDevice>> devices) {
    sendDevices(devices);
  }

} // module local


/* */

void registerHooks() {
  process::OnBrowser.attach(onBrowserProcess);
  process::OnRender.attach(onRenderProcess);

  // Hook notifications to get the browser-side client to render processes when they're created
  ui::OnClient.attach(onWebClient); // ovrly UI browser instance
  web::OnClient.attach(onWebClient); // instances for ovrlies

  // Hook VR notifications
  vr::OnReady.attach(onVRReady);
  vr::OnDevicesUpdated.attach(onDevicesUpdated);
}

}} // module exports
