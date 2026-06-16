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

    bool initializePipeline(int fd, uint32_t node_id);
    bool startCapture();
    void stopCapture();
    bool isCapturing() const { return is_capturing_; }

    bool restartPipeline(int fd);

    // Reemplaza solo el webrtcbin (para nueva negociación WebRTC tras
    // desconexión del cliente) manteniendo pipewiresrc y el encoder corriendo,
    // evitando así el reconecte a PipeWire que causaba pantalla negra.
    bool restartWebRtcBin();

    int getStreamWidth() const { return stream_w_; }
    int getStreamHeight() const { return stream_h_; }

    // WebRTC signaling: usado por WebRtcSignaling para negociar SDP/ICE con el cliente
    void setOnLocalDescription(LocalDescriptionCallback callback) { on_local_description_ = callback; }
    void setOnIceCandidate(IceCandidateCallback callback) { on_ice_candidate_ = callback; }
    void createOffer();
    void setRemoteDescription(const std::string& sdp);
    void addIceCandidate(guint mlineindex, const std::string& candidate);

private:
    // Puerto fijo de ICE-TCP para el medio WebRTC (adb reverse tcp:9006 tcp:9006)
    static constexpr guint kIceTcpPort = 9006;

    // Pipeline elements
    GstElement* pipeline_{nullptr};
    GstElement* pipewiresrc_{nullptr};
    GstElement* queue_main_{nullptr};
    GstElement* convert_{nullptr};
    GstElement* encoder_{nullptr};
    GstElement* h264parse_{nullptr};
    GstElement* rtph264pay_{nullptr};
    GstElement* webrtcbin_{nullptr};
    GstPad*     webrtc_sink_pad_{nullptr}; // request pad de webrtcbin (sink_%u)

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

    // Force-IDR al reanudar: detecta gaps en el stream y fuerza un keyframe limpio
    std::chrono::steady_clock::time_point last_frame_time_;
    static constexpr auto kStallThreshold = std::chrono::milliseconds(2000); // stall real, no gaps normales por pantalla estática

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

    // vah264enc/vaapih264enc admiten memory:DMABuf y conversión por GPU
    // (vapostproc/vaapipostproc), evitando el round-trip CPU<->GPU de videoconvert
    bool isVaapiEncoder() const
    {
        return encoder_name_ == "vah264enc" || encoder_name_ == "vaapih264enc";
    }

    static GstBusSyncReply onBusMessage(GstBus* bus, GstMessage* message, gpointer data);
    static GstPadProbeReturn onCapsProbe(GstPad* pad, GstPadProbeInfo* info, gpointer user_data);
    static GstPadProbeReturn onPayloaderBuffer(GstPad* pad, GstPadProbeInfo* info, gpointer user_data);
    static void onOfferCreated(GstPromise* promise, gpointer user_data);
    static void onIceCandidateCb(GstElement* webrtcbin, guint mlineindex, gchar* candidate, gpointer user_data);

    void cleanup();
    void error(const std::string& message);
};

#endif // GSTREAMER_MANAGER_H
