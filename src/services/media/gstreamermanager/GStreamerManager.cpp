#include "GStreamerManager.h"
#include <gst/video/video.h>
#include <iostream>
#include <cstring>
#include <string>
#include <algorithm>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>

GStreamerManager::GStreamerManager() {}

GStreamerManager::~GStreamerManager() {
    stopCapture();
    cleanup();
}

void GStreamerManager::initializeGStreamer() {
    gst_init(nullptr, nullptr);
}

bool GStreamerManager::initializePipeline(int fd, uint32_t node_id) {
    if (fd < 0 || node_id == 0) {
        error("Invalid file descriptor or node ID");
        return false;
    }

    if (pipeline_) {
        stopCapture();
        cleanup();
    }

    fd_ = fd;
    node_id_ = node_id;

    std::cout << "🎬 Initializing GStreamer pipeline...\n";

    if (!createElements() ||
        !configurePipeWireSource() ||
        !linkElements() ||
        !setupBusHandler()) {
        cleanup();
        return false;
    }

    std::cout << "✅ Pipeline initialized\n";
    return true;
}

bool GStreamerManager::createElements() {
    std::cout << "  Creating elements...\n";

    pipewiresrc_  = gst_element_factory_make("pipewiresrc",  "source");
    queue_main_   = gst_element_factory_make("queue",        "queue_main");
    videoconvert_ = gst_element_factory_make("videoconvert", "convert");
    h264parse_       = gst_element_factory_make("h264parse",    "parser");
    appsink_         = gst_element_factory_make("appsink",      "sink");

    // Encoder: HW primero, SW fallback
    struct Candidate { const char* name; const char* label; };
    static const Candidate candidates[] = {
        {"vah264enc",    "Intel/AMD H.264 VA-API"},
        {"vaapih264enc", "Intel/AMD H.264 VAAPI"},
        {"nvh264enc",    "NVIDIA H.264 NVENC"},
        {"x264enc",      "H.264 x264 software (zerolatency)"},
        {"openh264enc",  "H.264 OpenH264 software"},
        {nullptr, nullptr}
    };

    for (int i = 0; candidates[i].name && !encoder_; i++) {
        encoder_ = gst_element_factory_make(candidates[i].name, "encoder");
        if (!encoder_) continue;

        std::cout << "  ✓ Encoder: " << candidates[i].label << "\n";
        const std::string name = candidates[i].name;

        if (name == "vah264enc") {
            g_object_set(G_OBJECT(encoder_),
                         "bitrate",       config_.bitrate,
                         "key-int-max",   config_.keyframe_interval,
                         "target-usage",  config_.encoder_speed,
                         "rate-control",  8,
                         "cpb-size",      config_.bitrate / 8,
                         "ref-frames",    1,
                         "b-frames",      0,
                         NULL);
        } else if (name == "vaapih264enc") {
            g_object_set(G_OBJECT(encoder_),
                         "bitrate",         config_.bitrate,
                         "keyframe-period", config_.keyframe_interval,
                         "quality-level",   config_.encoder_speed,
                         NULL);
        } else if (name == "nvh264enc") {
            g_object_set(G_OBJECT(encoder_),
                         "bitrate",      config_.bitrate,
                         "preset",       6,
                         "rc-mode",      2,
                         "zerolatency",  TRUE,
                         NULL);
        } else if (name == "x264enc") {
            // speed-preset: 1=ultrafast ... 8=veryslow; map encoder_speed 1=quality→7, 7=speed→1
            int x264_speed = std::max(1, 8 - config_.encoder_speed);
            g_object_set(G_OBJECT(encoder_),
                         "tune",         0x00000004,
                         "speed-preset", x264_speed,
                         "bitrate",      config_.bitrate,
                         "key-int-max",  config_.keyframe_interval,
                         "threads",      4,
                         "bframes",      0,
                         "byte-stream",  TRUE,
                         "aud",          FALSE,
                         NULL);
        } else if (name == "openh264enc") {
            int complexity = (config_.encoder_speed <= 3) ? 2 : (config_.encoder_speed <= 6) ? 1 : 0;
            g_object_set(G_OBJECT(encoder_),
                         "bitrate",      config_.bitrate * 1000,  // bps
                         "complexity",   complexity,
                         "rate-control", 0,
                         NULL);
        }
    }

    if (!pipewiresrc_ || !queue_main_ || !videoconvert_ ||
        !encoder_ || !h264parse_ || !appsink_) {
        error("Failed to create one or more elements");
        if (!encoder_) std::cerr << "  ❌ No H.264 encoder available\n";
        if (!appsink_) std::cerr << "  ❌ appsink not available\n";
        return false;
    }

    // Queue: leaky=2 (downstream) descarta frames viejos, mantiene el más reciente
    g_object_set(G_OBJECT(queue_main_),
                 "max-size-buffers", 1, "max-size-bytes", 0,
                 "max-size-time", 0, "leaky", 2, NULL);

    // h264parse: SPS/PPS antes de cada IDR, formato Annex-B para MediaCodec
    g_object_set(G_OBJECT(h264parse_), "config-interval", -1, NULL);

    // appsink: sin sync, emit-signals para callback por cada buffer
    g_object_set(G_OBJECT(appsink_),
                 "emit-signals", TRUE,
                 "sync", FALSE,
                 "max-buffers", 1,
                 "drop", TRUE,
                 NULL);
    g_signal_connect(appsink_, "new-sample", G_CALLBACK(onNewSample), this);

    std::cout << "  ✓ All elements created\n";
    return true;
}

bool GStreamerManager::configurePipeWireSource() {
    g_object_set(G_OBJECT(pipewiresrc_),
                 "fd",   fd_,
                 "path", g_strdup_printf("%u", node_id_),
                 NULL);
    std::cout << "  ✓ PipeWire configured (fd=" << fd_ << ", node_id=" << node_id_ << ")\n";
    return true;
}


bool GStreamerManager::linkElements() {
    std::cout << "  Linking pipeline...\n";

    pipeline_ = gst_pipeline_new("skreenapp-pipeline");

    gst_bin_add_many(GST_BIN(pipeline_),
                     pipewiresrc_, queue_main_, videoconvert_,
                     encoder_, h264parse_, appsink_,
                     NULL);

    if (!gst_element_link_many(pipewiresrc_, queue_main_, videoconvert_,
                               encoder_, h264parse_, appsink_,
                               NULL)) {
        error("Failed to link pipeline elements");
        return false;
    }

    std::cout << "  ✓ Pipeline linked\n";
    return true;
}

bool GStreamerManager::setupBusHandler() {
    GstBus* bus = gst_element_get_bus(pipeline_);
    if (!bus) { error("Failed to get bus"); return false; }
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)onBusMessage, this, NULL);
    gst_object_unref(bus);
    std::cout << "  ✓ Bus handler set\n";
    return true;
}

// ============================================================
// TCP server raw (sin GDP)
// ============================================================

void GStreamerManager::startTcpServer() {
    server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd_ < 0) {
        std::cerr << "❌ No se pudo crear socket TCP\n";
        return;
    }

    int opt = 1;
    setsockopt(server_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr{};
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(9002);

    if (bind(server_fd_, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        std::cerr << "❌ bind() falló en puerto 9002\n";
        close(server_fd_);
        server_fd_ = -1;
        return;
    }

    listen(server_fd_, 5);
    server_running_ = true;
    accept_thread_ = std::thread(&GStreamerManager::acceptLoop, this);
    std::cout << "🌐 TCP server escuchando en puerto 9002\n";
}

void GStreamerManager::stopTcpServer() {
    server_running_ = false;
    if (server_fd_ >= 0) {
        shutdown(server_fd_, SHUT_RDWR);
        close(server_fd_);
        server_fd_ = -1;
    }
    if (accept_thread_.joinable()) accept_thread_.join();

    std::lock_guard<std::mutex> lock(clients_mutex_);
    std::cout << "🔌 Cerrando " << client_fds_.size() << " cliente(s) activo(s)\n";
    for (int fd : client_fds_) close(fd);
    client_fds_.clear();
    bytes_sent_total_ = 0;
    frames_sent_ = 0;
}

void GStreamerManager::acceptLoop() {
    std::cout << "🔁 acceptLoop iniciado, esperando conexiones...\n";
    while (server_running_) {
        struct sockaddr_in client_addr{};
        socklen_t len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (struct sockaddr*)&client_addr, &len);
        if (client_fd < 0) {
            if (server_running_)
                std::cerr << "⚠️  accept() error: " << strerror(errno) << "\n";
            break;
        }

        char ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip, sizeof(ip));
        uint16_t port = ntohs(client_addr.sin_port);

        int flag = 1;
        setsockopt(client_fd, IPPROTO_TCP, TCP_NODELAY, &flag, sizeof(flag));

        // Buffer suficiente para IDR frames del HW encoder sin corrupción
        int sndbuf = 262144;
        setsockopt(client_fd, SOL_SOCKET, SO_SNDBUF, &sndbuf, sizeof(sndbuf));

        std::lock_guard<std::mutex> lock(clients_mutex_);
        client_fds_.push_back(client_fd);
        std::cout << "📱 Cliente conectado  ip=" << ip << ":" << port
                  << "  fd=" << client_fd
                  << "  total=" << client_fds_.size() << "\n";
    }
    std::cout << "🔁 acceptLoop terminado\n";
}

void GStreamerManager::sendToClients(const uint8_t* data, gsize size) {
    std::lock_guard<std::mutex> lock(clients_mutex_);

    if (client_fds_.empty()) return;

    std::vector<int> to_remove;

    // Prefijo de 4 bytes (big-endian) con el tamaño del frame H.264
    uint8_t len_prefix[4] = {
        static_cast<uint8_t>((size >> 24) & 0xFF),
        static_cast<uint8_t>((size >> 16) & 0xFF),
        static_cast<uint8_t>((size >>  8) & 0xFF),
        static_cast<uint8_t>( size        & 0xFF),
    };

    for (int fd : client_fds_) {
        send(fd, len_prefix, 4, MSG_NOSIGNAL);
        ssize_t sent = send(fd, data, size, MSG_NOSIGNAL);
        if (sent < 0) {
            std::cout << "📴 Cliente desconectado  fd=" << fd
                      << "  error=" << strerror(errno) << "\n";
            close(fd);
            to_remove.push_back(fd);
        }
    }

    for (int fd : to_remove) {
        client_fds_.erase(
            std::remove(client_fds_.begin(), client_fds_.end(), fd),
            client_fds_.end());
    }

    bytes_sent_total_ += size;

    // Log de estadísticas cada 300 frames (~10 segundos a 30fps)
    if (frames_sent_ % 300 == 0) {
        double mb = bytes_sent_total_ / (1024.0 * 1024.0);
        std::cout << "📊 TCP stats  frames=" << frames_sent_
                  << "  total=" << mb << " MB"
                  << "  clientes=" << client_fds_.size() << "\n";
    }
}

GstFlowReturn GStreamerManager::onNewSample(GstElement* appsink, gpointer user_data) {
    auto* self = static_cast<GStreamerManager*>(user_data);

    GstSample* sample = gst_app_sink_pull_sample(GST_APP_SINK(appsink));
    if (!sample) return GST_FLOW_OK;

    // Read stream dimensions from caps (only once)
    if (!self->stream_size_known_) {
        GstCaps* caps = gst_sample_get_caps(sample);
        if (caps) {
            GstStructure* s = gst_caps_get_structure(caps, 0);
            int w = 0, h = 0;
            gst_structure_get_int(s, "width", &w);
            gst_structure_get_int(s, "height", &h);
            if (w > 0 && h > 0) {
                self->stream_w_ = w;
                self->stream_h_ = h;
                self->stream_size_known_ = true;
                std::cout << "Stream size: " << w << "x" << h << "\n";
            }
        }
    }

    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstMapInfo map;
    if (gst_buffer_map(buffer, &map, GST_MAP_READ)) {
        // Log del primer sample (solo una vez)
        if (self->frames_sent_++ == 0) {
            std::cout << "🎞️  Primer sample  size=" << map.size << " bytes"
                      << "  primeros bytes: ";
            for (gsize i = 0; i < std::min(map.size, (gsize)8); i++)
                std::printf("%02X ", map.data[i]);
            std::printf("\n");
            std::cout << "   (H.264 Annex-B: primeros bytes deben ser 00 00 00 01)\n";
        }

        auto now = std::chrono::steady_clock::now();
        bool was_stalled = (now - self->last_frame_time_) >= kStallThreshold;
        self->last_frame_time_ = now;

        // Si el pipeline estuvo parado (PipeWire inactivo), forzar IDR inmediato
        // para que el móvil reciba un frame limpio sin artefactos de referencia
        if (was_stalled)
            self->forceKeyframe();

        self->sendToClients(map.data, map.size);
        gst_buffer_unmap(buffer, &map);
    }

    gst_sample_unref(sample);
    return GST_FLOW_OK;
}

// ============================================================

bool GStreamerManager::startCapture() {
    if (is_capturing_) return true;
    if (!pipeline_) { error("Pipeline not initialized"); return false; }

    last_frame_time_ = std::chrono::steady_clock::now();
    watchdog_running_ = true;
    watchdog_thread_ = std::thread(&GStreamerManager::watchdogLoop, this);
    startTcpServer();

    std::cout << "▶️ Starting pipeline...\n";
    if (gst_element_set_state(pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE) {
        error("Failed to start pipeline");
        return false;
    }

    is_capturing_ = true;
    std::cout << "✅ Streaming on TCP port 9002\n";
    return true;
}

void GStreamerManager::stopCapture() {
    if (!is_capturing_ || !pipeline_) return;
    watchdog_running_ = false;
    if (watchdog_thread_.joinable()) watchdog_thread_.join();
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    is_capturing_ = false;
    stopTcpServer();
    std::cout << "⏹️ Pipeline stopped\n";
}

GstBusSyncReply GStreamerManager::onBusMessage(GstBus* bus, GstMessage* message, gpointer data) {
    (void)bus;
    GStreamerManager* self = static_cast<GStreamerManager*>(data);

    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError* err;
            gchar* debug_info;
            gst_message_parse_error(message, &err, &debug_info);
            std::cerr << "GStreamer Error: " << err->message << "\n";
            if (debug_info) std::cerr << "   Debug: " << debug_info << "\n";
            g_clear_error(&err);
            g_free(debug_info);
            self->error("GStreamer error occurred");
            break;
        }
        case GST_MESSAGE_EOS:
            std::cout << "End of stream\n";
            break;
        case GST_MESSAGE_STATE_CHANGED:
            if (GST_MESSAGE_SRC(message) == GST_OBJECT(self->pipeline_)) {
                GstState old_state, new_state;
                gst_message_parse_state_changed(message, &old_state, &new_state, NULL);
                std::cout << "Pipeline: " << gst_element_state_get_name(old_state)
                          << " -> " << gst_element_state_get_name(new_state) << "\n";
            }
            break;
        default:
            break;
    }
    return GST_BUS_PASS;
}

void GStreamerManager::watchdogLoop() {
    auto last_cycle = std::chrono::steady_clock::now() - std::chrono::seconds(10);

    while (watchdog_running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~1 frame a 60fps
        if (!is_capturing_ || !pipewiresrc_) continue;

        auto now = std::chrono::steady_clock::now();

        if ((now - last_frame_time_) < kStallThreshold) continue;

        // Cooldown: ciclar como máximo una vez por segundo mientras haya stall.
        // Cada ciclo fuerza al compositor a entregar el frame actual de pantalla,
        // evitando que el usuario tenga que mover el mouse tras cambiar de ventana.
        if ((now - last_cycle) < std::chrono::milliseconds(50)) continue;

        last_cycle = now;
        std::cout << "⏸️ Watchdog: stall, ciclando pipewiresrc\n";

        gst_element_set_locked_state(pipewiresrc_, TRUE);
        gst_element_set_state(pipewiresrc_, GST_STATE_PAUSED);
        gst_element_set_state(pipewiresrc_, GST_STATE_PLAYING);
        gst_element_set_locked_state(pipewiresrc_, FALSE);
    }
}

void GStreamerManager::forceKeyframe() {
    if (!encoder_) return;
    // Envía evento upstream al encoder para generar un IDR en el próximo frame
    GstPad* sink_pad = gst_element_get_static_pad(encoder_, "sink");
    if (sink_pad) {
        GstEvent* event = gst_video_event_new_upstream_force_key_unit(
            GST_CLOCK_TIME_NONE, TRUE, 0);
        gst_pad_push_event(sink_pad, event);
        gst_object_unref(sink_pad);
        std::cout << "🔑 IDR forzado tras reanudación de stream\n";
    }
}

void GStreamerManager::cleanup() {
    if (pipeline_) {
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        pipewiresrc_ = queue_main_ = videoconvert_ = nullptr;
        encoder_ = h264parse_ = appsink_ = nullptr;
    }
}

void GStreamerManager::error(const std::string& message) {
    std::cerr << "GStreamer Error: " << message << "\n";
    is_capturing_ = false;
    if (error_callback_) error_callback_(message);
}
