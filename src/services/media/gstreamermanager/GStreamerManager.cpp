#include "GStreamerManager.h"
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
        !configureFilters() ||
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

    pipewiresrc_     = gst_element_factory_make("pipewiresrc",  "source");
    videorate_       = gst_element_factory_make("videorate",    "rate");
    capsfilter_rate_ = gst_element_factory_make("capsfilter",   "filter_rate");
    queue_main_      = gst_element_factory_make("queue",        "queue_main");
    videoscale_      = gst_element_factory_make("videoscale",   "scale");
    capsfilter_scale_= gst_element_factory_make("capsfilter",   "filter_scale");
    videoconvert_    = gst_element_factory_make("videoconvert", "convert");
    h264parse_       = gst_element_factory_make("h264parse",    "parser");
    mpegtsmux_       = gst_element_factory_make("mpegtsmux",   "muxer");
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
                         "bitrate", 4000,
                         "key-int-max", 15,
                         "target-usage", 7,
                         "rate-control", 8,   // VCM: Video Conferencing Mode, baja latencia
                         "ref-frames", 1,      // mínimas referencias = menor latencia encoder
                         "b-frames", 0,
                         NULL);
        } else if (name == "vaapih264enc") {
            g_object_set(G_OBJECT(encoder_),
                         "bitrate", 4000, "keyframe-period", 30, "quality-level", 7, NULL);
        } else if (name == "nvh264enc") {
            g_object_set(G_OBJECT(encoder_),
                         "bitrate", 4000, "preset", 6, "rc-mode", 2, "zerolatency", TRUE, NULL);
        } else if (name == "x264enc") {
            g_object_set(G_OBJECT(encoder_),
                         "tune", 0x00000004, "speed-preset", 1, "bitrate", 4000,
                         "key-int-max", 15, "threads", 4, "bframes", 0,
                         "byte-stream", TRUE, "aud", FALSE, NULL);
        } else if (name == "openh264enc") {
            g_object_set(G_OBJECT(encoder_),
                         "bitrate", 4000000, "complexity", 0, "rate-control", 0, NULL);
        }
    }

    if (!pipewiresrc_ || !videorate_ || !capsfilter_rate_ || !queue_main_ ||
        !videoscale_ || !capsfilter_scale_ || !videoconvert_ ||
        !encoder_ || !h264parse_ || !mpegtsmux_ || !appsink_) {
        error("Failed to create one or more elements");
        if (!encoder_)   std::cerr << "  ❌ No H.264 encoder available\n";
        if (!mpegtsmux_) std::cerr << "  ❌ mpegtsmux not available\n";
        if (!appsink_)   std::cerr << "  ❌ appsink not available\n";
        return false;
    }

    // Queue: leaky=2 (downstream) descarta frames viejos, mantiene el más reciente
    g_object_set(G_OBJECT(queue_main_),
                 "max-size-buffers", 1, "max-size-bytes", 0,
                 "max-size-time", 0, "leaky", 2, NULL);

    // h264parse: SPS/PPS antes de cada IDR
    g_object_set(G_OBJECT(h264parse_), "config-interval", -1, NULL);

    // mpegtsmux: 7 TS packets por buffer (balance latencia/eficiencia)
    g_object_set(G_OBJECT(mpegtsmux_), "alignment", 7, NULL);

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
                 "fd", fd_,
                 "path", g_strdup_printf("%u", node_id_),
                 NULL);
    std::cout << "  ✓ PipeWire configured (fd=" << fd_ << ", node_id=" << node_id_ << ")\n";
    return true;
}

bool GStreamerManager::configureFilters() {
    g_object_set(G_OBJECT(videorate_), "drop-only", TRUE, NULL);
    GstCaps* caps_rate = gst_caps_new_simple("video/x-raw",
                                             "framerate", GST_TYPE_FRACTION, 30, 1, NULL);
    g_object_set(G_OBJECT(capsfilter_rate_), "caps", caps_rate, NULL);
    gst_caps_unref(caps_rate);

    GstCaps* caps_scale = gst_caps_new_simple("video/x-raw",
                                              "width",  G_TYPE_INT, 1280,
                                              "height", G_TYPE_INT, 720, NULL);
    g_object_set(G_OBJECT(capsfilter_scale_), "caps", caps_scale, NULL);
    gst_caps_unref(caps_scale);

    std::cout << "  ✓ Filters configured (30fps, 1280x720)\n";
    return true;
}

bool GStreamerManager::linkElements() {
    std::cout << "  Linking pipeline...\n";

    pipeline_ = gst_pipeline_new("skreenapp-pipeline");

    gst_bin_add_many(GST_BIN(pipeline_),
                     pipewiresrc_, videorate_, capsfilter_rate_, queue_main_,
                     videoscale_, capsfilter_scale_, videoconvert_,
                     encoder_, h264parse_, mpegtsmux_, appsink_,
                     NULL);

    if (!gst_element_link_many(pipewiresrc_, videorate_, capsfilter_rate_, queue_main_,
                               videoscale_, capsfilter_scale_, videoconvert_,
                               encoder_, h264parse_, mpegtsmux_, appsink_,
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

        // Leer y descartar la petición HTTP del cliente
        char req[2048] = {};
        recv(client_fd, req, sizeof(req) - 1, 0);

        const char* headers =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: video/mp2t\r\n"
            "Connection: close\r\n"
            "Cache-Control: no-cache\r\n"
            "\r\n";
        send(client_fd, headers, strlen(headers), MSG_NOSIGNAL);

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

    for (int fd : client_fds_) {
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
            std::cout << "   (MPEG-TS: primer byte debe ser 0x47)\n";
        }
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

void GStreamerManager::cleanup() {
    if (pipeline_) {
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        pipewiresrc_ = videorate_ = capsfilter_rate_ = queue_main_ = nullptr;
        videoscale_ = capsfilter_scale_ = videoconvert_ = nullptr;
        encoder_ = h264parse_ = mpegtsmux_ = appsink_ = nullptr;
    }
}

void GStreamerManager::error(const std::string& message) {
    std::cerr << "GStreamer Error: " << message << "\n";
    is_capturing_ = false;
    if (error_callback_) error_callback_(message);
}
