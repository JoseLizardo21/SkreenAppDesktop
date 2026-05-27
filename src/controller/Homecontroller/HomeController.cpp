#include "HomeController.h"
#include "../../views/home/Home.h"
#include "../../views/settings/Settings.h"
#include "../../services/system/portalmanager/PortalManager.h"
#include <memory>
#include <iostream>
#include <cstdlib>
#include <glib.h>

static bool firewalld_port_opened = false;

static bool isFirewalldActive() {
    return std::system("systemctl is-active --quiet firewalld 2>/dev/null") == 0;
}

static void openFirewallPort() {
    if (!isFirewalldActive()) return;
    if (std::system("firewall-cmd --query-port=9002/tcp --zone=public -q 2>/dev/null") == 0 &&
        std::system("firewall-cmd --query-port=9003/tcp --zone=public -q 2>/dev/null") == 0 &&
        std::system("firewall-cmd --query-port=9004/tcp --zone=public -q 2>/dev/null") == 0) return;
    if (std::system("pkexec /usr/local/lib/skreenapp/skreenapp-firewall-helper open 2>/dev/null") == 0) {
        firewalld_port_opened = true;
        std::cout << "Ports 9002, 9003, 9004 opened in firewalld\n";
    }
}

static void closeFirewallPort() {
    // Runtime rules (no --permanent) disappear on firewalld restart or reboot,
    // so closing actively is not required and avoids an extra pkexec dialog.
    if (!firewalld_port_opened) return;
    std::system("firewall-cmd --remove-port=9002/tcp --remove-port=9003/tcp --remove-port=9004/tcp --zone=public -q 2>/dev/null");
    firewalld_port_opened = false;
    std::cout << "Ports 9002, 9003, 9004 closed in firewalld\n";
}

HomeController::HomeController(Home* home)
    : view_(home) {
    config_ = config_manager_.load();

    gstreamer_manager_ = std::make_unique<GStreamerManager>();
    portal_manager_ = std::make_unique<PortalManager>();

    portal_manager_->setPortalCallback(
        [this](const std::string& session_handle, uint32_t node_id, int fd) {
            onPortalComplete(session_handle, node_id, fd);
        });
    view_->setOnRequestPermissionsCallback([this]() { handleRequestPermissions(); });
    view_->setOnCancelTransmissionCallback([this]() { handleStopCapture(); });
    view_->setOnSettingsCallback([this]() { handleOpenSettings(); });

    adb_monitor_ = std::make_unique<AdbMonitor>();
    adb_monitor_->setOnDeviceConnected([this]() {
        device_connected_ = true;
        auto* v = view_;
        g_idle_add([](gpointer data) -> gboolean {
            static_cast<Home*>(data)->setDeviceConnected(true);
            static_cast<Home*>(data)->setTransmitButtonEnabled(true);
            return G_SOURCE_REMOVE;
        }, v);
    });
    adb_monitor_->setOnDeviceDisconnected([this]() {
        device_connected_ = false;
        auto* v = view_;
        g_idle_add([](gpointer data) -> gboolean {
            static_cast<Home*>(data)->setDeviceConnected(false);
            static_cast<Home*>(data)->setTransmitButtonEnabled(false);
            return G_SOURCE_REMOVE;
        }, v);
    });
    adb_monitor_->start();
}

HomeController::~HomeController() {
    if (stop_thread_.joinable()) stop_thread_.join();
    closeFirewallPort();
}

void HomeController::handleRequestPermissions() {
    openFirewallPort();
    if (portal_manager_) portal_manager_->startAsync();
}

void HomeController::onPortalComplete(const std::string& session_handle,
                                      uint32_t node_id, int fd) {
    (void)session_handle;

    if (!gstreamer_manager_) return;

    gstreamer_manager_->setConfig(config_);
    if (!gstreamer_manager_->initializePipeline(fd, node_id)) {
        onGStreamerError("Failed to initialize pipeline");
        return;
    }

    if (!gstreamer_manager_->startCapture()) {
        onGStreamerError("Failed to start capture");
        return;
    }

    notify_server_ = std::make_unique<NotifyServer>();
    notify_server_->start();

    input_server_ = std::make_unique<InputServer>();
    auto* pm = portal_manager_.get();
    auto* gm = gstreamer_manager_.get();

    // Touch state: 0=idle, 1=down(potential tap), 2=dragging
    struct TouchCtx { int state{0}; float sx{}, sy{}; };
    auto ctx = std::make_shared<TouchCtx>();

    input_server_->start([pm, gm, ctx](uint8_t type, int32_t, float nx, float ny) {
        int w = gm->getStreamWidth();
        int h = gm->getStreamHeight();
        double ax = nx * w;
        double ay = ny * h;

        if (type == 1) {                          // touch down
            ctx->state = 1;
            ctx->sx = nx; ctx->sy = ny;
            pm->notifyPointerMotionAbsolute(ax, ay);

        } else if (type == 0) {                   // move (1 finger)
            pm->notifyPointerMotionAbsolute(ax, ay);
            if (ctx->state == 1) {
                float dx = nx - ctx->sx, dy = ny - ctx->sy;
                if (dx*dx + dy*dy > 0.0001f) {   // ~1% of screen = drag threshold
                    ctx->state = 2;
                    pm->notifyPointerButton(272, 1); // button down: start drag
                }
            }

        } else if (type == 2) {                   // touch up
            if (ctx->state == 1) {
                pm->notifyPointerButton(272, 1);  // tap: atomic click
                pm->notifyPointerButton(272, 0);
            } else if (ctx->state == 2) {
                pm->notifyPointerButton(272, 0);  // end drag
            }
            ctx->state = 0;

        } else if (type == 3) {                   // 2-finger scroll (ny = normalized delta)
            double scroll = ny * 500.0;           // scale: full swipe ≈ 500px scroll
            pm->notifyPointerAxis(0.0, scroll);
        }
    });

    if (view_) {
        auto* v = view_;
        g_idle_add([](gpointer data) -> gboolean {
            static_cast<Home*>(data)->setTransmitting(true);
            return G_SOURCE_REMOVE;
        }, v);
    }
    std::cout << "Streaming TCP en puerto 9002\n";
    std::cout << "Input control TCP en puerto 9003\n";
    std::cout << "Notify TCP en puerto 9004\n";
    std::cout << "   Conectar con: adb reverse tcp:9002 tcp:9002 && adb reverse tcp:9003 tcp:9003 && adb reverse tcp:9004 tcp:9004\n";
}

void HomeController::onGStreamerError(const std::string& error) {
    std::cerr << "Error: " << error << "\n";
    handleStopCapture();
}


void HomeController::handleOpenSettings() {
    auto* s = new Settings(view_->getGtkWindow(), config_);
    s->setOnSaveCallback([this](StreamConfig cfg) {
        config_ = cfg;
        config_manager_.save(cfg);
        std::cout << "Settings saved: bitrate=" << cfg.bitrate
                  << " keyframe=" << cfg.keyframe_interval
                  << " speed=" << cfg.encoder_speed << "\n";
    });
    s->show();
}

void HomeController::handleStopCapture() {
    // Update UI immediately from the GTK main loop (safe from any thread)
    struct Ctx { Home* view; bool connected; };
    auto* ctx = new Ctx{view_, device_connected_};
    g_idle_add([](gpointer data) -> gboolean {
        auto* c = static_cast<Ctx*>(data);
        if (c->view) {
            c->view->setTransmitting(false);
            c->view->setDeviceConnected(c->connected);
            c->view->setTransmitButtonEnabled(c->connected);
        }
        delete c;
        return G_SOURCE_REMOVE;
    }, ctx);

    // Run blocking cleanup off the GTK main thread
    if (stop_thread_.joinable()) stop_thread_.join();
    stop_thread_ = std::thread([this]() {
        if (notify_server_) { notify_server_->send("{\"type\":\"stream_stopped\"}\n"); notify_server_->stop(); notify_server_.reset(); }
        if (input_server_) { input_server_->stop(); input_server_.reset(); }
        if (gstreamer_manager_) gstreamer_manager_->stopCapture();
        if (portal_manager_) portal_manager_->stop();
    });
}
