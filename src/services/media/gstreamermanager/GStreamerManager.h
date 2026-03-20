#ifndef GSTREAMER_MANAGER_H
#define GSTREAMER_MANAGER_H

#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>
#include <functional>
#include <cstdint>
#include <memory>
#include <string>


class WebSocketClient;

class GStreamerManager {
public:
    using FrameCallback = std::function<void(const uint8_t*, int, int)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    GStreamerManager(WebSocketClient* ws_client);
    ~GStreamerManager();

    static void initializeGStreamer();

    void setErrorCallback(ErrorCallback callback) { error_callback_ = callback; }

    bool initializePipeline(int fd, uint32_t node_id);
    bool startCapture();
    void stopCapture();
    bool isCapturing() const { return is_capturing_; }

    // ========== NUEVO: WebRTC ==========
    /**
     * Enable WebRTC streaming
     * @param signaling_server: URL del servidor de señalización (ej: "ws://localhost:9001")
     * @return true if successful
     */
    bool enableWebRTC(const std::string& signaling_server);
    
    /**
     * Disable WebRTC streaming
     */
    void disableWebRTC();
    
    /**
     * Check if WebRTC is active
     */
    bool isWebRTCActive() const { return webrtc_enabled_; }

private:
    // Pipeline elements existentes
    GstElement* pipeline_{nullptr};
    GstElement* pipewiresrc_{nullptr};
    GstElement* videoconvert1_{nullptr};
    GstElement* videorate_{nullptr};
    GstElement* capsfilter_rate_{nullptr};
    GstElement* videoscale_{nullptr};
    GstElement* capsfilter_scale_{nullptr};
    GstElement* videoconvert2_{nullptr};

    // ========== NUEVO: WebRTC elements ==========
    GstElement* queue_webrtc_{nullptr};
    GstElement* webrtcbin_{nullptr};
    GstElement* x264enc_{nullptr};
    GstElement* rtph264pay_{nullptr};
    GstElement* videoconvert_webrtc_{nullptr};
    GstElement* videoscale_webrtc_{nullptr};
    GstElement* capsfilter_webrtc_{nullptr};
    
    bool webrtc_enabled_{false};
    std::string signaling_server_;
    WebSocketClient* ws_client_{nullptr};

    // State existente
    bool is_capturing_{false};
    int fd_{-1};
    uint32_t node_id_{0};

    FrameCallback frame_callback_;
    ErrorCallback error_callback_;

    // Pipeline setup existente
    bool createElements();
    bool configurePipeWireSource();
    bool configureFrameRateFilter();
    bool linkElements();
    bool setupBusHandler();

    // ========== NUEVO: WebRTC setup ==========
    bool createWebRTCElements();
    bool linkWebRTCBranch();
    void setupWebRTCCallbacks();
    void connectToSignalingServer();
    
    // WebRTC signal handlers
    static void onNegotiationNeeded(GstElement* webrtc, gpointer user_data);
    static void onIceCandidate(GstElement* webrtc, guint mlineindex, 
                               gchar* candidate, gpointer user_data);
    static void onOfferCreated(GstPromise* promise, gpointer user_data);
    
    // Signaling handlers
    void sendSDP(const std::string& type, const std::string& sdp);
    void sendICECandidate(guint mline_index, const std::string& candidate);
    void handleSignalingMessage(const std::string& message);
    void handleRemoteSDP(const std::string& type, const std::string& sdp);
    void handleRemoteICE(guint mline_index, const std::string& candidate);

    // Callbacks existentes
    static GstBusSyncReply onBusMessage(GstBus* bus, GstMessage* message, gpointer data);

    void cleanup();
    void error(const std::string& message);
    void handleSample(GstSample* sample);
};

#endif // GSTREAMER_MANAGER_H