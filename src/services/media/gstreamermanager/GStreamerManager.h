#ifndef GSTREAMER_MANAGER_H
#define GSTREAMER_MANAGER_H

#include <gst/gst.h>
#include <functional>
#include <cstdint>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include "config/StreamConfig.h"
#include "config/ConnectionMode.h"

// Forward declaration instead of including <gst/webrtc/webrtc.h> (which requires
// GST_USE_UNSTABLE_API) just for the type of a callback parameter.
typedef struct _GstWebRTCDTLSTransport GstWebRTCDTLSTransport;

class GStreamerManager {
public:
    using ErrorCallback = std::function<void(const std::string&)>;
    using LocalDescriptionCallback = std::function<void(const std::string& sdp)>;
    using IceCandidateCallback = std::function<void(guint mlineindex, const std::string& candidate)>;

    GStreamerManager();
    ~GStreamerManager();

    static void initializeGStreamer();

    void setErrorCallback(ErrorCallback callback) { error_callback_ = callback; }
    void setConfig(const StreamConfig& cfg) { config_ = cfg; }
    void setConnectionMode(ConnectionMode mode) { connection_mode_ = mode; }

    bool initializePipeline(int fd, uint32_t node_id);
    bool startCapture();
    void stopCapture();
    bool isCapturing() const { return is_capturing_; }

    bool restartPipeline(int fd);

    // Replaces only the webrtcbin (for a fresh WebRTC negotiation after the
    // client disconnects) while keeping pipewiresrc and the encoder running,
    // avoiding the PipeWire reconnect that caused a black screen.
    bool restartWebRtcBin();

    int getStreamWidth() const { return stream_w_; }
    int getStreamHeight() const { return stream_h_; }

    // WebRTC signaling: used by WebRtcSignaling to negotiate SDP/ICE with the client
    void setOnLocalDescription(LocalDescriptionCallback callback) { on_local_description_ = callback; }
    void setOnIceCandidate(IceCandidateCallback callback) { on_ice_candidate_ = callback; }
    void createOffer();
    void setRemoteDescription(const std::string& sdp);
    void addIceCandidate(guint mlineindex, const std::string& candidate);

private:
    // Fixed ICE-TCP port for the WebRTC media (adb reverse tcp:9006 tcp:9006)
    static constexpr guint kIceTcpPort = 9006;
    // UDP port range for ICE in WiFi mode (see the firewall note in the plan)
    static constexpr guint kIceWifiUdpPortMin = 40000;
    static constexpr guint kIceWifiUdpPortMax = 40020;
    // RFC4588 retransmission PT for H.264 (PT 96, see the rtp_out caps in linkElements)
    static constexpr guint kRtxPayloadType = 97;

    ConnectionMode connection_mode_{ConnectionMode::Cable};

    // Pipeline elements
    GstElement* pipeline_{nullptr};
    GstElement* pipewiresrc_{nullptr};
    GstElement* queue_main_{nullptr};
    GstElement* convert_{nullptr};
    GstElement* encoder_{nullptr};
    GstElement* h264parse_{nullptr};
    GstElement* rtph264pay_{nullptr};
    GstElement* webrtcbin_{nullptr};
    GstPad*     webrtc_sink_pad_{nullptr}; // webrtcbin request pad (sink_%u)

    std::string encoder_name_;
    StreamConfig config_;

    bool is_capturing_{false};
    int fd_{-1};
    uint32_t node_id_{0};

    int stream_w_{1920};
    int stream_h_{1080};
    bool stream_size_known_{false};

    ErrorCallback error_callback_;
    LocalDescriptionCallback on_local_description_;
    IceCandidateCallback on_ice_candidate_;

    // Force-IDR on resume: detects gaps in the stream and forces a clean keyframe
    std::chrono::steady_clock::time_point last_frame_time_;
    static constexpr auto kStallThreshold = std::chrono::milliseconds(2000); // a real stall, not the normal gaps of a static screen

    void forceKeyframe();

    // Watchdog: when Wayland sends no frames (idle window), forces a RECONFIGURE on
    // pipewiresrc so the compositor delivers the current screen state
    std::thread watchdog_thread_;
    std::atomic<bool> watchdog_running_{false};
    void watchdogLoop();

    bool createElements();
    bool configurePipeWireSource();
    bool linkElements();
    bool setupBusHandler();
    void configureIceForMode(GObject* ice_agent);

    // vah264enc/vaapih264enc support memory:DMABuf and GPU conversion
    // (vapostproc/vaapipostproc), avoiding videoconvert's CPU<->GPU round-trip
    bool isVaapiEncoder() const
    {
        return encoder_name_ == "vah264enc" || encoder_name_ == "vaapih264enc";
    }

    static GstBusSyncReply onBusMessage(GstBus* bus, GstMessage* message, gpointer data);
    static GstPadProbeReturn onCapsProbe(GstPad* pad, GstPadProbeInfo* info, gpointer user_data);
    static GstPadProbeReturn onPayloaderBuffer(GstPad* pad, GstPadProbeInfo* info, gpointer user_data);
    static void onOfferCreated(GstPromise* promise, gpointer user_data);
    static void onIceCandidateCb(GstElement* webrtcbin, guint mlineindex, gchar* candidate, gpointer user_data);
    static void onIceConnectionStateCb(GstElement* webrtcbin, GParamSpec* pspec, gpointer user_data);
    // NACK/RTX (RFC4588): over WiFi, unlike cable (ICE-TCP, which retransmits at
    // the transport level), lost UDP packets are gone for good unless the sender
    // retransmits them on request. This handler creates the rtprtxsend bin that
    // webrtcbin inserts into the send chain when it asks for one.
    static GstElement* onRequestAuxSender(GstElement* webrtcbin, GstWebRTCDTLSTransport* transport, gpointer user_data);

    void cleanup();
    void error(const std::string& message);
};

#endif // GSTREAMER_MANAGER_H
