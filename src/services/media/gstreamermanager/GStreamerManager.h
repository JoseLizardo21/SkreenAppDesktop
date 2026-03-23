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

class GStreamerManager {
public:
    using ErrorCallback = std::function<void(const std::string&)>;

    GStreamerManager();
    ~GStreamerManager();

    static void initializeGStreamer();

    void setErrorCallback(ErrorCallback callback) { error_callback_ = callback; }

    bool initializePipeline(int fd, uint32_t node_id);
    bool startCapture();
    void stopCapture();
    bool isCapturing() const { return is_capturing_; }

private:
    // Pipeline elements
    GstElement* pipeline_{nullptr};
    GstElement* pipewiresrc_{nullptr};
    GstElement* videorate_{nullptr};
    GstElement* capsfilter_rate_{nullptr};
    GstElement* queue_main_{nullptr};
    GstElement* videoscale_{nullptr};
    GstElement* capsfilter_scale_{nullptr};
    GstElement* videoconvert_{nullptr};
    GstElement* encoder_{nullptr};
    GstElement* h264parse_{nullptr};
    GstElement* appsink_{nullptr};

    bool is_capturing_{false};
    int fd_{-1};
    uint32_t node_id_{0};

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
    static constexpr auto kStallThreshold = std::chrono::milliseconds(200);

    void forceKeyframe();

    bool createElements();
    bool configurePipeWireSource();
    bool configureFilters();
    bool linkElements();
    bool setupBusHandler();

    static GstFlowReturn onNewSample(GstElement* appsink, gpointer user_data);
    static GstBusSyncReply onBusMessage(GstBus* bus, GstMessage* message, gpointer data);

    void cleanup();
    void error(const std::string& message);
};

#endif // GSTREAMER_MANAGER_H
