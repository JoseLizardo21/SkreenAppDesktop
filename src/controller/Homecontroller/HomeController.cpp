#include "HomeController.h"
#include "../../views/home/Home.h"
#include "../../services/system/portalmanager/PortalManager.h"
#include <memory>
#include <iostream>
#include <cstdlib>

static bool firewalld_port_opened = false;

static bool isFirewalldActive() {
    return std::system("systemctl is-active --quiet firewalld 2>/dev/null") == 0;
}

static void openFirewallPort() {
    if (!isFirewalldActive()) return;
    if (std::system("firewall-cmd --query-port=9002/tcp --zone=public -q 2>/dev/null") == 0) return;
    if (std::system("pkexec firewall-cmd --add-port=9002/tcp --zone=public 2>/dev/null") == 0) {
        firewalld_port_opened = true;
        std::cout << "🔓 Puerto 9002 abierto en firewalld\n";
    }
}

static void closeFirewallPort() {
    if (!firewalld_port_opened) return;
    std::system("pkexec firewall-cmd --remove-port=9002/tcp --zone=public 2>/dev/null");
    firewalld_port_opened = false;
    std::cout << "🔒 Puerto 9002 cerrado en firewalld\n";
}

HomeController::HomeController(Home* home)
    : view_(home) {
    gstreamer_manager_ = std::make_unique<GStreamerManager>();
    portal_manager_ = std::make_unique<PortalManager>();

    portal_manager_->setPortalCallback(
        [this](const std::string& session_handle, uint32_t node_id, int fd) {
            onPortalComplete(session_handle, node_id, fd);
        });
    view_->setOnRequestPermissionsCallback([this]() { handleRequestPermissions(); });
    view_->setOnCancelTransmissionCallback([this]() { handleStopCapture(); });
    view_->setTransmitButtonEnabled(true);
}

HomeController::~HomeController() {}

void HomeController::handleRequestPermissions() {
    openFirewallPort();
    if (portal_manager_) portal_manager_->startAsync();
}

void HomeController::onPortalComplete(const std::string& session_handle,
                                      uint32_t node_id, int fd) {
    (void)session_handle;

    if (!gstreamer_manager_) return;

    if (!gstreamer_manager_->initializePipeline(fd, node_id)) {
        onGStreamerError("Failed to initialize pipeline");
        return;
    }

    if (!gstreamer_manager_->startCapture()) {
        onGStreamerError("Failed to start capture");
        return;
    }

    input_server_ = std::make_unique<InputServer>();
    auto* pm = portal_manager_.get();
    auto* gm = gstreamer_manager_.get();

    input_server_->start([pm, gm](uint8_t type, int32_t, float nx, float ny) {
        int w = gm->getStreamWidth();
        int h = gm->getStreamHeight();
        double ax = nx * w;
        double ay = ny * h;
        if (type == 1) {
            // Atomic click: DOWN+UP in the same callback so the button never gets stuck
            pm->notifyPointerMotionAbsolute(ax, ay);
            pm->notifyPointerButton(272, 1);
            pm->notifyPointerButton(272, 0);
        } else if (type == 0) {
            pm->notifyPointerMotionAbsolute(ax, ay);
        }
        // type==2 ignored: button already released atomically on type==1
    });

    if (view_) view_->setTransmitting(true);
    std::cout << "Streaming TCP en puerto 9002\n";
    std::cout << "Input control TCP en puerto 9003\n";
    std::cout << "   Conectar con: adb reverse tcp:9002 tcp:9002 && adb reverse tcp:9003 tcp:9003\n";
}

void HomeController::onGStreamerError(const std::string& error) {
    std::cerr << "Error: " << error << "\n";
    handleStopCapture();
}


void HomeController::handleStopCapture() {
    closeFirewallPort();

    if (view_) view_->setTransmitting(false);

    if (input_server_) {
        input_server_->stop();
        input_server_.reset();
    }

    if (gstreamer_manager_) {
        gstreamer_manager_->stopCapture();
    }

    if (portal_manager_) portal_manager_->stop();
}
