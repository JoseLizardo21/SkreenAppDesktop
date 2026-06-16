#include "HomeController.h"
#include "../../views/home/Home.h"
#include "../../views/settings/Settings.h"
#include "../../services/system/portalmanager/PortalManager.h"
#include <memory>
#include <iostream>
#include <glib.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

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
}

void HomeController::handleRequestPermissions() {
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

    webrtc_signaling_ = std::make_unique<WebRtcSignaling>();
    auto* ws = webrtc_signaling_.get();
    auto* gm = gstreamer_manager_.get();

    gstreamer_manager_->setOnLocalDescription([ws](const std::string& sdp) {
        json j;
        j["type"] = "offer";
        j["sdp"] = sdp;
        ws->send(j.dump() + "\n");
    });

    gstreamer_manager_->setOnIceCandidate([ws](guint mlineindex, const std::string& candidate) {
        json j;
        j["type"] = "ice";
        j["candidate"] = candidate;
        j["sdpMLineIndex"] = mlineindex;
        ws->send(j.dump() + "\n");
    });

    webrtc_signaling_->setOnClientConnected([gm]() {
        gm->createOffer();
    });

    session_active_->store(true);
    auto active = session_active_;
    webrtc_signaling_->setOnClientDisconnected([gm, active]() {
        if (!active->load())
            return;
        // Solo reemplazamos el webrtcbin, dejando pipewiresrc y el encoder
        // corriendo para evitar el reconecte a PipeWire que causaba pantalla negra.
        if (!gm->restartWebRtcBin())
            std::cerr << "[HomeController] Error al reiniciar el WebRTC bin\n";
    });

    webrtc_signaling_->setOnMessage([gm](const std::string& line) {
        try {
            json j = json::parse(line);
            std::string type = j.value("type", "");
            if (type == "answer") {
                gm->setRemoteDescription(j.value("sdp", ""));
            } else if (type == "ice") {
                guint mline = j.value("sdpMLineIndex", 0);
                std::string candidate = j.value("candidate", "");
                gm->addIceCandidate(mline, candidate);
            }
        } catch (const std::exception& e) {
            std::cerr << "[WebRtcSignaling] Invalid message: " << e.what() << "\n";
        }
    });

    webrtc_signaling_->start();

    input_server_ = std::make_unique<InputServer>();
    auto* pm = portal_manager_.get();

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
    std::cout << "WebRTC signaling TCP en puerto 9002\n";
    std::cout << "Input control TCP en puerto 9003\n";
    std::cout << "Notify TCP en puerto 9004\n";
    std::cout << "WebRTC media (ICE-TCP) en puerto 9006\n";
    std::cout << "   Conectar con: adb reverse tcp:9002 tcp:9002 && adb reverse tcp:9003 tcp:9003 && adb reverse tcp:9004 tcp:9004 && adb reverse tcp:9006 tcp:9006\n";
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
    // Cancela cualquier reconexión pendiente antes de detener los servicios,
    // evitando que el worker de PortalManager llame a restartPipeline() sobre
    // un GStreamerManager ya destruido.
    session_active_->store(false);

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
        if (webrtc_signaling_) { webrtc_signaling_->stop(); webrtc_signaling_.reset(); }
        if (input_server_) { input_server_->stop(); input_server_.reset(); }
        if (gstreamer_manager_) gstreamer_manager_->stopCapture();
        if (portal_manager_) portal_manager_->stop();
    });
}
