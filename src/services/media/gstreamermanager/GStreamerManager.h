#ifndef GSTREAMER_MANAGER_H
#define GSTREAMER_MANAGER_H

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <functional>
#include <cstdint>
#include <string>
#include <thread>
#include <mutex>
#include <vector>
#include <atomic>
#include <chrono>
#include "config/StreamConfig.h"

class GStreamerManager {
public:
    using ErrorCallback = std::function<void(const std::string&)>;

    GStreamerManager();
    ~GStreamerManager();

    static void initializeGStreamer();

    void setErrorCallback(ErrorCallback callback) { error_callback_ = callback; }
    void setConfig(const StreamConfig& cfg) { config_ = cfg; }

    bool initializePipeline(int fd, uint32_t node_id);
    bool startCapture();
    void stopCapture();
    bool isCapturing() const { return is_capturing_; }

    int getStreamWidth() const { return stream_w_; }
    int getStreamHeight() const { return stream_h_; }

private:
    // Pipeline elements
    GstElement* pipeline_{nullptr};
    GstElement* pipewiresrc_{nullptr};
    GstElement* queue_main_{nullptr};
    GstElement* videoconvert_{nullptr};
    GstElement* encoder_{nullptr};
    GstElement* h264parse_{nullptr};
    GstElement* appsink_{nullptr};

    StreamConfig config_;

    bool is_capturing_{false};
    int fd_{-1};
    uint32_t node_id_{0};

    int stream_w_{1920};
    int stream_h_{1080};
    bool stream_size_known_{false};

    ErrorCallback error_callback_;

    // TCP server raw (sin GDP)
    int server_fd_{-1};
    std::vector<int> client_fds_;
    std::mutex clients_mutex_;
    std::thread accept_thread_;
    std::atomic<bool> server_running_{false};
    uint64_t bytes_sent_total_{0};
    uint64_t frames_sent_{0};

    void startTcpServer();
    void stopTcpServer();
    void acceptLoop();
    void sendToClients(const uint8_t* data, gsize size);

    // Force-IDR al reanudar: detecta gaps en el stream y fuerza un keyframe limpio
    std::chrono::steady_clock::time_point last_frame_time_;
    static constexpr auto kStallThreshold = std::chrono::milliseconds(66); // 2 frames a 30fps

    void forceKeyframe();

    // Watchdog: cuando Wayland no envía frames (ventana idle), fuerza un RECONFIGURE
    // en pipewiresrc para que el compositor entregue el estado actual de pantalla
    std::thread watchdog_thread_;
    std::atomic<bool> watchdog_running_{false};
    void watchdogLoop();

    bool createElements();
    bool configurePipeWireSource();
    bool linkElements();
    bool setupBusHandler();

    static GstFlowReturn onNewSample(GstElement* appsink, gpointer user_data);
    static GstBusSyncReply onBusMessage(GstBus* bus, GstMessage* message, gpointer data);

    void cleanup();
    void error(const std::string& message);
};

#endif // GSTREAMER_MANAGER_H
