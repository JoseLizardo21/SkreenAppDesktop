#include "HomeController.h"
#include "../../views/home/Home.h"
#include "../../services/system/portalmanager/PortalManager.h"
#include "../../services/websocketclient/WebSocketClient.h"
#include <memory>
#include <iostream>

HomeController::HomeController(Home* home, WebSocketClient* ws)
    : view_(home), ws_(ws) {
    portal_manager_ = std::make_unique<PortalManager>();
    gstreamer_manager_ = std::make_unique<GStreamerManager>(ws_);

    portal_manager_->setPortalCallback(
        [this](const std::string& session_handle, uint32_t node_id, int fd) {
            onPortalComplete(session_handle, node_id, fd);
        });
    view_->setOnRequestPermissionsCallback(
        [this]() { handleRequestPermissions(); });
    ws_->setTransmitButtonEnabled(
        [this](bool isEnabled){enabledButtonTransmiter(isEnabled);});
}

HomeController::~HomeController() {
    // handleStopCapture();
}

void HomeController::handleRequestPermissions() {

    if (portal_manager_) {
        portal_manager_->startAsync();
    }
}

void HomeController::onPortalComplete(const std::string& session_handle,
                                         uint32_t node_id,
                                         int fd) {
    // std::cout << "Portal workflow complete\n";
    // std::cout << "   Session: " << session_handle << "\n";
    // std::cout << "   Node ID: " << node_id << "\n";
    // std::cout << "   FD: " << fd << "\n";
    
    // Update capture session
    // if (capture_session_) {
    //     capture_session_->onPortalComplete(session_handle, node_id, fd);
    // }

    // Initialize GStreamer pipeline with portal data
    if (gstreamer_manager_) {
        if (gstreamer_manager_->initializePipeline(fd, node_id)) {
            std::cout << "🎬 GStreamer pipeline initialized\n";

            // Start GStreamer capture
            if (gstreamer_manager_->startCapture()) {
                std::cout << "▶️ GStreamer capture started\n";

                // Enable WebRTC streaming
                if (gstreamer_manager_->enableWebRTC("ws://localhost:9001")) {
                    std::cout << "🌐 WebRTC enabled and connected to signaling server\n";
                } else {
                    std::cerr << "⚠️ Failed to enable WebRTC, continuing with local preview only\n";
                }
            } else {
                onGStreamerError("Failed to start GStreamer capture");
            }
        } else {
            onGStreamerError("Failed to initialize GStreamer pipeline");
        }
    }
}

void HomeController::enabledButtonTransmiter(const bool isEnabled) {
    if(view_) {
        view_->setTransmitButtonEnabled(isEnabled);
    }

    // When device disconnects, stop capture sessions
    if (!isEnabled) {
        handleStopCapture();
    }
}

void HomeController::onGStreamerError(const std::string& error) {
    std::cerr << "GStreamer error: " << error << "\n";

    handleStopCapture();

    if (view_) {
        // Could show error dialog here
    }
}

void HomeController::handleStopCapture() {
    std::cout << "Stopping capture\n";

    // Disable WebRTC first
    if (gstreamer_manager_) {
        gstreamer_manager_->disableWebRTC();
        gstreamer_manager_->stopCapture();
    }

    // Stop portal workflow
    if (portal_manager_) {
        portal_manager_->stop();
    }
}