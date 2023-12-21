/*
 * This file is part of ovrly (https://github.com/joshperry/ovrly)
 * Copyright (c) 2020 Joshua Perry
 *
 * This program can be redistributed and/or modified under the
 * terms of the GNU General Public License as published by
 * the Free Software Foundation, version 3.
 */
#include "jsovrly.h"

#include <string>
#include <functional>
#include <thread>

#include <zmq.hpp>

#include "openvr.h"
#include "appovrly.h"
#include "webovrly.h"
#include "uiovrly.h"
#include "logging.h"
#include "serralize.hpp"

namespace ovr = ::vr;

using namespace std::string_literals;
auto const readonly = CefV8Value::PropertyAttribute::V8_PROPERTY_ATTRIBUTE_READONLY;

namespace ovrly { namespace js {

// Module local
namespace {

  // ZMQ context for browser _and_ render process zmq connections
  zmq::context_t zctx_{ 1 };
  const char* ZMQ_URL = "tcp://127.0.0.1:9995";
  std::unique_ptr<zmq::socket_t> zsock_;
  const char* ZMQ_RPC_URL = "tcp://127.0.0.1:9996";
  std::unique_ptr<zmq::socket_t> rpcsock_;

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

      // And the zmq rep socket
      rpcsock_ = std::make_unique<zmq::socket_t>(zctx_, zmq::socket_type::rep);
      try {
        rpcsock_->bind(ZMQ_RPC_URL);
      } catch(zmq::error_t& err) {
        logger::error("Problem binding zmq rpc listener: {}", err.what());
      }
    });
  }

  void loadDev(CefRefPtr<CefV8Value> const& jsdevs, vr::TrackedDevice &device) {
    const wchar_t *name = L"none";

    switch(device.type) {
    case ovr::TrackedDeviceClass_HMD:
      name = L"hmd";
      break;
    case ovr::TrackedDeviceClass_Controller:
      switch(device.role.value()) {
        case ovr::TrackedControllerRole_LeftHand:
          name = L"left";
          break;
        case ovr::TrackedControllerRole_RightHand:
          name = L"right";
          break;
        case ovr::TrackedControllerRole_Invalid:
          return;
        default:
          // Not right or left, don't export
          logger::debug("(js) Skipped controller {}", device.role.value());
          return;
      }
      break;
    default:
      // If it's not the HMD or controllers, let's not export
      return;
    }

    if(!jsdevs->HasValue(name)) {
      auto dev = CefV8Value::CreateObject(nullptr, nullptr);
      dev->SetValue(L"manufacturer", CefV8Value::CreateString(device.manufacturer), readonly);
      dev->SetValue(L"model", CefV8Value::CreateString(device.model), readonly);
      dev->SetValue(L"serial", CefV8Value::CreateString(device.serial), readonly);
      jsdevs->SetValue(name, dev, readonly);
    }

    auto dev = jsdevs->GetValue(name);
    dev->SetValue(L"connected", CefV8Value::CreateBool(device.connected), readonly);

    if(device.pose) {
      if(!dev->HasValue(L"pose")) {
        // ovrly.devices.hmd.pose.matrix
        auto devmat = CefV8Value::CreateArray(12);
        auto devpose = CefV8Value::CreateObject(nullptr, nullptr);
        devpose->SetValue(L"matrix", devmat, readonly);
        dev->SetValue(L"pose", devpose, readonly);
      }

      auto devpose = dev->GetValue(L"pose");
      auto devmat = devpose->GetValue(L"matrix");

      for(int i = 0; i < 12; i++) {
        devmat->SetValue(i, CefV8Value::CreateDouble(device.pose.value().matrix[i]));
      }
    }
  }

  // When the render process is being created
  void onRenderProcess(process::Render& rp) {
    logger::debug("(js) Render process spawned!");
    // Setup the zmq IPC connection and inject the js API interface.
    rp.SubOnContextCreated.attach([](CefRefPtr<CefBrowser> browser, auto frame, CefRefPtr<CefV8Context> jsctx) {
      // TODO: Setup the js API
      // The js setup is currently identical for both the UI client and the overlie clients,
      // but future versions may want to differentiate the js interface between the two.
      logger::debug("(js) Render process spawned JS context!");

      auto ovrly = CefV8Value::CreateObject(nullptr, nullptr);
      ovrly->SetValue(L"initialized", CefV8Value::CreateBool(true), readonly);
      jsctx->GetGlobal()->SetValue(L"ovrly", ovrly, readonly);

      // Get the bound info from the VR subsystem


      // Start a thread to receive zmq IPC messages
      zloop_ = std::make_unique<std::thread>([jsctx]() {
        logger::debug("(js) Spawned ZMQ message loop thread");

        try {
          // Setup and connect the zmq sub socket
          zsock_ = std::make_unique<zmq::socket_t>(zctx_, zmq::socket_type::sub);
          zsock_->connect(ZMQ_URL);

          zsock_->set(zmq::sockopt::subscribe, "vr.devices");

          while(1) {
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
                process::runOnMain([jsctx, topic, msg]() {
                  // Deserialize the message data
                  serralize::InMemStream instream(msg->data(), msg->size());
                  std::vector<vr::TrackedDevice> devices;
                  deserialize(instream, &devices);

                  jsctx->Enter();

                  auto ovrly = jsctx->GetGlobal()->GetValue(L"ovrly");
                  if(!ovrly->HasValue(L"devices")) {
                    // ovrly.devices.hmd
                    auto jsdevs = CefV8Value::CreateObject(nullptr, nullptr);

                    // ovrly.devices
                    ovrly->SetValue(L"devices", jsdevs, readonly);
                  }

                  auto jsdevs = ovrly->GetValue(L"devices");

                  for(auto &dev: devices) {
                    loadDev(jsdevs, dev);
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

  /**
   * Serializes some message object into a zmq message and sets up cleanup
   */
  template<typename T>
  zmq::message_t getZmqMessage(const T &data) {
    // TODO: should we create a cache of outstreams to limit allocations?
    auto outstream = new serralize::OutMemStream();
    serialize(*outstream, data);

    return {
      &outstream->getBuf()[0],
      outstream->getBuf().size(),
      [](void* buf, void* sharedref) { delete static_cast<serralize::OutMemStream*>(sharedref); },
      outstream
    };
  }

  /**
   * Send a serializable message object to each subscribed render process
   */
  template<typename T>
  void publishMessage(const std::string &topic, const T &data) {
    try {
      // Send the envelope topic part
      zsock_->send(zmq::message_t(topic), zmq::send_flags::sndmore);

      // Serialize the data and send it as the envelope body
      auto msg = getZmqMessage(data);
      zsock_->send(std::move(msg), zmq::send_flags::none);
    } catch(zmq::error_t &err) {
      logger::error("Sending zmq message failed: {}", err.what());
    }
  }

  // Send an updated device state list to the each render process
  void sendDevices(std::shared_ptr<const std::vector<vr::TrackedDevice>> devices) {
    publishMessage("vr.devices.updated", *devices.get());
  }

  // When the VR module is ready to go, on main thread
  void onVRReady() {
    auto devices = std::make_shared<std::vector<vr::TrackedDevice>>(vr::getDevices());
    sendDevices(devices);
//    auto playbounds = vr::getPlaybounds();
//    setPlaybounds(playbounds);
  }

  // When we receive a device state update from the VR module, on main thread
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
