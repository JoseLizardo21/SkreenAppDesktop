#include "GStreamerManager.h"
#include <gst/video/video.h>
#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>
#include <gst/sdp/sdp.h>
#include <nice/agent.h>
#include <iostream>
#include <string>
#include <mutex>
#include <condition_variable>

GStreamerManager::GStreamerManager() {}

GStreamerManager::~GStreamerManager()
{
    stopCapture();
    cleanup();
}

void GStreamerManager::initializeGStreamer()
{
    gst_init(nullptr, nullptr);
}

bool GStreamerManager::initializePipeline(int fd, uint32_t node_id)
{
    if (fd < 0 || node_id == 0)
    {
        error("Invalid file descriptor or node ID");
        return false;
    }

    if (pipeline_)
    {
        stopCapture();
        cleanup();
    }

    fd_ = fd;
    node_id_ = node_id;

    std::cout << "🎬 Initializing GStreamer pipeline...\n";

    if (!createElements() ||
        !configurePipeWireSource() ||
        !linkElements() ||
        !setupBusHandler())
    {
        cleanup();
        return false;
    }

    std::cout << "✅ Pipeline initialized\n";
    return true;
}

bool GStreamerManager::createElements()
{
    std::cout << "  Creating elements...\n";

    pipewiresrc_ = gst_element_factory_make("pipewiresrc", "source");
    queue_main_ = gst_element_factory_make("queue", "queue_main");
    h264parse_ = gst_element_factory_make("h264parse", "parser");
    rtph264pay_ = gst_element_factory_make("rtph264pay", "pay");
    webrtcbin_ = gst_element_factory_make("webrtcbin", "webrtcbin");

    // Encoder: hardware first, software fallback
    struct Candidate
    {
        const char *name;
        const char *label;
    };
    static const Candidate candidates[] = {
        {"vah264enc", "Intel/AMD H.264 VA-API"},
        {"vaapih264enc", "Intel/AMD H.264 VAAPI"},
        {"nvh264enc", "NVIDIA H.264 NVENC"},
        {"x264enc", "H.264 x264 software (zerolatency)"},
        {"openh264enc", "H.264 OpenH264 software"},
        {nullptr, nullptr}};

    for (int i = 0; candidates[i].name && !encoder_; i++)
    {
        encoder_ = gst_element_factory_make(candidates[i].name, "encoder");
        if (!encoder_)
            continue;

        std::cout << "  ✓ Encoder: " << candidates[i].label << "\n";
        const std::string name = candidates[i].name;
        encoder_name_ = name;

        if (name == "vah264enc")
        {
            std::cout << "  Hereeeeeee1" << "\n";
            g_object_set(G_OBJECT(encoder_),
                         "bitrate", config_.bitrate,
                         "key-int-max", config_.keyframe_interval,
                         "target-usage", config_.encoder_speed,
                         "rate-control", 8,
                         "cpb-size", config_.bitrate / 8,
                         "ref-frames", 1,
                         "b-frames", 0,
                         NULL);
        }
        else if (name == "vaapih264enc")
        {
            std::cout << "  Hereeeeeee2" << "\n";
            g_object_set(G_OBJECT(encoder_),
                         "bitrate", config_.bitrate,
                         "keyframe-period", config_.keyframe_interval,
                         "quality-level", config_.encoder_speed,
                         "max-bframes", 0,
                         "cpb-length", 125, // ms; equivalent to vah264enc's cpb-size (bitrate/8)
                         NULL);

            // CBR via the string nick: avoids depending on the enum's numeric
            // values, which change between gstreamer-vaapi plugin versions.
            gst_util_set_object_arg(G_OBJECT(encoder_), "rate-control", "cbr");
        }
        else if (name == "nvh264enc")
        {
            std::cout << "  Hereeeeeee3" << "\n";
            g_object_set(G_OBJECT(encoder_),
                         "bitrate", config_.bitrate,
                         "preset", 6,
                         "rc-mode", 2,
                         "zerolatency", TRUE,
                         NULL);
        }
        else if (name == "x264enc")
        {
            std::cout << "  Hereeeeeee4" << "\n";
            g_object_set(G_OBJECT(encoder_),
                         "tune", 0x00000004,
                         "speed-preset", 1,
                         "bitrate", config_.bitrate,
                         "key-int-max", config_.keyframe_interval,
                         "threads", 4,
                         "bframes", 0,
                         "byte-stream", TRUE,
                         "aud", FALSE,
                         "profile", 0,
                         NULL);
        }
        else if (name == "openh264enc")
        {
            std::cout << "  Hereeeeeee5" << "\n";
            int complexity = (config_.encoder_speed <= 3) ? 2 : (config_.encoder_speed <= 6) ? 1
                                                                                             : 0;
            g_object_set(G_OBJECT(encoder_),
                         "bitrate", config_.bitrate * 1000, // bps
                         "complexity", complexity,
                         "rate-control", 0,
                         NULL);
        }
    }

    // Pre-encoder conversion: with VAAPI/VA use vapostproc/vaapipostproc (GPU),
    // which can take memory:DMABuf from pipewiresrc and hand the surface to the
    // encoder already on the GPU. Otherwise, videoconvert (CPU) over system memory.
    if (encoder_name_ == "vah264enc")
        convert_ = gst_element_factory_make("vapostproc", "convert");
    else if (encoder_name_ == "vaapih264enc")
        convert_ = gst_element_factory_make("vaapipostproc", "convert");
    else
        convert_ = gst_element_factory_make("videoconvert", "convert");

    if (!pipewiresrc_ || !queue_main_ || !convert_ ||
        !encoder_ || !h264parse_ || !rtph264pay_ || !webrtcbin_)
    {
        error("Failed to create one or more elements");
        if (!encoder_)
            std::cerr << "  ❌ No H.264 encoder available\n";
        if (!webrtcbin_)
            std::cerr << "  ❌ webrtcbin not available\n";
        return false;
    }

    // Queue: leaky=2 (downstream) drops old frames and keeps the most recent one
    g_object_set(G_OBJECT(queue_main_),
                 "max-size-buffers", 1, "max-size-bytes", 0,
                 "max-size-time", 0, "leaky", 2, NULL);

    // h264parse: SPS/PPS before every IDR
    g_object_set(G_OBJECT(h264parse_),
                 "config-interval", -1,
                 "disable-passthrough", TRUE,
                 NULL);

    // rtph264pay: repeat SPS/PPS on every IDR so a client joining mid-stream can
    // decode from the first keyframe
    g_object_set(G_OBJECT(rtph264pay_),
                 "config-interval", -1,
                 "pt", 96,
                 NULL);

    // webrtcbin: a single video m-line, no STUN/TURN (direct LAN or loopback over adb)
    g_object_set(G_OBJECT(webrtcbin_),
                 "bundle-policy", 3 /* GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE */,
                 NULL);

    GObject *ice_agent = nullptr;
    g_object_get(G_OBJECT(webrtcbin_), "ice-agent", &ice_agent, NULL);
    if (ice_agent)
    {
        configureIceForMode(ice_agent);
        g_object_unref(ice_agent);
    }

    g_signal_connect(webrtcbin_, "on-ice-candidate", G_CALLBACK(onIceCandidateCb), this);
    g_signal_connect(webrtcbin_, "notify::ice-connection-state", G_CALLBACK(onIceConnectionStateCb), this);

    std::cout << "  ✓ All elements created\n";
    return true;
}

void GStreamerManager::configureIceForMode(GObject *ice_agent)
{
    if (connection_mode_ == ConnectionMode::Cable)
    {
        // Force ICE-TCP: adb reverse/forward only tunnel TCP, not UDP. UDP candidate
        // gathering is disabled and the TCP port is pinned to kIceTcpPort so it can be
        // forwarded with a single "adb reverse tcp:9006 tcp:9006".
        g_object_set(ice_agent,
                     "ice-udp", FALSE,
                     "ice-tcp", TRUE,
                     "min-rtp-port", kIceTcpPort,
                     "max-rtp-port", kIceTcpPort,
                     NULL);

        // Restrict candidate gathering to loopback: without this, libnice
        // auto-detects every local interface (e.g. Wi-Fi) and offers TCP
        // candidates on those IPs. If the client picks one, the connection ends
        // up tied to that network instead of the adb (USB) tunnel.
        NiceAgent *nice_agent = nullptr;
        g_object_get(ice_agent, "agent", &nice_agent, NULL);
        if (nice_agent)
        {
            NiceAddress loopback;
            nice_address_init(&loopback);
            nice_address_set_from_string(&loopback, "127.0.0.1");
            nice_agent_add_local_address(nice_agent, &loopback);
            g_object_unref(nice_agent);
        }
    }
    else // ConnectionMode::Wifi
    {
        // UDP+TCP enabled: host candidates on every real network interface (no
        // loopback restriction), preferring UDP for lower latency. libnice
        // enumerates the local interfaces automatically.
        g_object_set(ice_agent,
                     "ice-udp", TRUE,
                     "ice-tcp", TRUE,
                     "min-rtp-port", kIceWifiUdpPortMin,
                     "max-rtp-port", kIceWifiUdpPortMax,
                     NULL);
    }
}

bool GStreamerManager::configurePipeWireSource()
{
    g_object_set(G_OBJECT(pipewiresrc_),
                 "fd", fd_,
                 "path", g_strdup_printf("%u", node_id_),
                 NULL);
    std::cout << "  ✓ PipeWire configured (fd=" << fd_ << ", node_id=" << node_id_ << ")\n";
    return true;
}

bool GStreamerManager::linkElements()
{
    std::cout << "  Linking pipeline...\n";

    pipeline_ = gst_pipeline_new("skreenapp-pipeline");

    // Capsfilter after pipewiresrc. With vapostproc/vaapipostproc (GPU) it is left
    // unconstrained: they can negotiate memory:DMABuf directly with pipewiresrc,
    // avoiding a CPU<->GPU copy per frame. With videoconvert (CPU) system memory
    // must be forced, since on GPU machines the screencast portal usually offers
    // memory:DMABuf, which videoconvert cannot negotiate.
    GstElement *src_caps = gst_element_factory_make("capsfilter", "src_caps");
    if (!isVaapiEncoder())
    {
        GstCaps *sys_mem_caps = gst_caps_new_empty_simple("video/x-raw");
        GstCapsFeatures *sys_mem_features = gst_caps_features_new(GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY, NULL);
        gst_caps_set_features(sys_mem_caps, 0, sys_mem_features);
        g_object_set(G_OBJECT(src_caps), "caps", sys_mem_caps, NULL);
        gst_caps_unref(sys_mem_caps);
    }

    // TEMPORARY DIAGNOSTIC: videorate/rate_caps disabled - it seems to be
    // blocking the buffers after the first one.
    // GstElement *videorate = gst_element_factory_make("videorate", "rate");
    // g_object_set(G_OBJECT(videorate), "drop-only", FALSE, "skip-to-first", TRUE, NULL);
    //
    // GstElement *rate_caps = gst_element_factory_make("capsfilter", "rate_caps");
    // GstCaps *rate_caps_val = gst_caps_new_simple("video/x-raw",
    //                                              "framerate", GST_TYPE_FRACTION, 30, 1,
    //                                              NULL);
    // g_object_set(G_OBJECT(rate_caps), "caps", rate_caps_val, NULL);
    // gst_caps_unref(rate_caps_val);

    // Pre-encoder capsfilter: x264enc only, forces I420+sRGB to avoid a green tint
    GstElement *enc_in = gst_element_factory_make("capsfilter", "enc_in");
    if (encoder_name_ == "x264enc")
    {
        GstCaps *yuv_caps = gst_caps_new_simple("video/x-raw",
                                                "format", G_TYPE_STRING, "I420",
                                                "colorimetry", G_TYPE_STRING, "bt709",
                                                NULL);
        g_object_set(G_OBJECT(enc_in), "caps", yuv_caps, NULL);
        gst_caps_unref(yuv_caps);
    }

    // Capsfilter forcing byte-stream (Annex-B) on the h264parse output
    GstElement *h264out = gst_element_factory_make("capsfilter", "h264_out");
    GstCaps *sink_caps = gst_caps_new_simple("video/x-h264",
                                             "stream-format", G_TYPE_STRING, "byte-stream",
                                             "alignment", G_TYPE_STRING, "au",
                                             NULL);
    g_object_set(G_OBJECT(h264out), "caps", sink_caps, NULL);
    gst_caps_unref(sink_caps);

    // Payloader output capsfilter: explicit RTP caps for webrtcbin's sink_%u
    GstElement *rtp_out = gst_element_factory_make("capsfilter", "rtp_out");
    GstCaps *rtp_caps = gst_caps_new_simple("application/x-rtp",
                                            "media", G_TYPE_STRING, "video",
                                            "encoding-name", G_TYPE_STRING, "H264",
                                            "payload", G_TYPE_INT, 96,
                                            "clock-rate", G_TYPE_INT, 90000,
                                            NULL);
    g_object_set(G_OBJECT(rtp_out), "caps", rtp_caps, NULL);
    gst_caps_unref(rtp_caps);

    gst_bin_add_many(GST_BIN(pipeline_),
                     pipewiresrc_, src_caps, queue_main_, convert_,
                     enc_in, encoder_, h264parse_, h264out,
                     rtph264pay_, rtp_out, webrtcbin_,
                     NULL);

    if (!gst_element_link_many(pipewiresrc_, src_caps, queue_main_, convert_,
                               enc_in, encoder_, h264parse_, h264out,
                               rtph264pay_, rtp_out,
                               NULL))
    {
        error("Failed to link pipeline elements");
        return false;
    }

    // rtp_out -> webrtcbin (request pad: creates a sendonly video transceiver)
    GstPad *rtp_src = gst_element_get_static_pad(rtp_out, "src");
    GstPad *webrtc_sink = gst_element_request_pad_simple(webrtcbin_, "sink_%u");
    if (!rtp_src || !webrtc_sink || gst_pad_link(rtp_src, webrtc_sink) != GST_PAD_LINK_OK)
    {
        error("Failed to link rtph264pay to webrtcbin");
        if (rtp_src)
            gst_object_unref(rtp_src);
        if (webrtc_sink)
            gst_object_unref(webrtc_sink);
        return false;
    }
    gst_object_unref(rtp_src);
    webrtc_sink_pad_ = webrtc_sink; // keep the ref so the bin can be swapped dynamically

    // Read the real width/height of the encoded stream (to map touch coordinates)
    GstPad *parse_src = gst_element_get_static_pad(h264parse_, "src");
    gst_pad_add_probe(parse_src, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
                      onCapsProbe, this, NULL);
    gst_object_unref(parse_src);

    // Activity marker for the stall watchdog (forceKeyframe after resuming)
    GstPad *pay_sink = gst_element_get_static_pad(rtph264pay_, "sink");
    gst_pad_add_probe(pay_sink, GST_PAD_PROBE_TYPE_BUFFER,
                      onPayloaderBuffer, this, NULL);
    gst_object_unref(pay_sink);

    std::cout << "  ✓ Pipeline linked\n";
    return true;
}

bool GStreamerManager::setupBusHandler()
{
    GstBus *bus = gst_element_get_bus(pipeline_);
    if (!bus)
    {
        error("Failed to get bus");
        return false;
    }
    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)onBusMessage, this, NULL);
    gst_object_unref(bus);
    std::cout << "  ✓ Bus handler set\n";
    return true;
}

GstPadProbeReturn GStreamerManager::onCapsProbe(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    (void)pad;
    auto *self = static_cast<GStreamerManager *>(user_data);
    if (self->stream_size_known_)
        return GST_PAD_PROBE_OK;

    GstEvent *event = GST_PAD_PROBE_INFO_EVENT(info);
    if (GST_EVENT_TYPE(event) != GST_EVENT_CAPS)
        return GST_PAD_PROBE_OK;

    GstCaps *caps;
    gst_event_parse_caps(event, &caps);
    GstStructure *s = gst_caps_get_structure(caps, 0);
    int w = 0, h = 0;
    if (gst_structure_get_int(s, "width", &w) && gst_structure_get_int(s, "height", &h) && w > 0 && h > 0)
    {
        self->stream_w_ = w;
        self->stream_h_ = h;
        self->stream_size_known_ = true;
        std::cout << "Stream size: " << w << "x" << h << "\n";
    }
    return GST_PAD_PROBE_OK;
}

GstPadProbeReturn GStreamerManager::onPayloaderBuffer(GstPad *pad, GstPadProbeInfo *info, gpointer user_data)
{
    (void)pad;
    (void)info;
    auto *self = static_cast<GStreamerManager *>(user_data);

    auto now = std::chrono::steady_clock::now();
    bool was_stalled = (now - self->last_frame_time_) >= kStallThreshold;
    self->last_frame_time_ = now;

    // If the pipeline was stalled (PipeWire idle), force an immediate IDR so the
    // client gets a clean frame with no reference artifacts
    if (was_stalled)
        self->forceKeyframe();

    return GST_PAD_PROBE_OK;
}

// ============================================================
// WebRTC signaling
// ============================================================

void GStreamerManager::createOffer()
{
    if (!webrtcbin_)
        return;
    GstPromise *promise = gst_promise_new_with_change_func(onOfferCreated, this, NULL);
    g_signal_emit_by_name(webrtcbin_, "create-offer", NULL, promise);
}

void GStreamerManager::onOfferCreated(GstPromise *promise, gpointer user_data)
{
    auto *self = static_cast<GStreamerManager *>(user_data);

    const GstStructure *reply = gst_promise_get_reply(promise);
    GstWebRTCSessionDescription *offer = nullptr;
    gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);
    gst_promise_unref(promise);

    if (!offer)
        return;

    g_signal_emit_by_name(self->webrtcbin_, "set-local-description", offer, NULL);

    gchar *sdp_str = gst_sdp_message_as_text(offer->sdp);
    if (self->on_local_description_)
        self->on_local_description_(std::string(sdp_str));
    g_free(sdp_str);

    gst_webrtc_session_description_free(offer);
}

void GStreamerManager::setRemoteDescription(const std::string &sdp)
{
    if (!webrtcbin_)
        return;

    GstSDPMessage *sdp_msg;
    gst_sdp_message_new(&sdp_msg);
    gst_sdp_message_parse_buffer(reinterpret_cast<const guint8 *>(sdp.c_str()), sdp.size(), sdp_msg);

    GstWebRTCSessionDescription *answer =
        gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER, sdp_msg);
    g_signal_emit_by_name(webrtcbin_, "set-remote-description", answer, NULL);
    gst_webrtc_session_description_free(answer);
}

void GStreamerManager::addIceCandidate(guint mlineindex, const std::string &candidate)
{
    if (!webrtcbin_)
        return;
    g_signal_emit_by_name(webrtcbin_, "add-ice-candidate", mlineindex, candidate.c_str());
}

void GStreamerManager::onIceCandidateCb(GstElement *webrtcbin, guint mlineindex, gchar *candidate, gpointer user_data)
{
    (void)webrtcbin;
    auto *self = static_cast<GStreamerManager *>(user_data);
    if (self->on_ice_candidate_)
        self->on_ice_candidate_(mlineindex, std::string(candidate));
}

void GStreamerManager::onIceConnectionStateCb(GstElement *webrtcbin, GParamSpec *, gpointer user_data)
{
    GstWebRTCICEConnectionState state;
    g_object_get(webrtcbin, "ice-connection-state", &state, NULL);
    // Force an IDR once ICE is ready to transmit: guarantees the first frame the
    // client receives is a decodable keyframe (with no prior reference).
    if (state == GST_WEBRTC_ICE_CONNECTION_STATE_CONNECTED ||
        state == GST_WEBRTC_ICE_CONNECTION_STATE_COMPLETED)
    {
        auto *self = static_cast<GStreamerManager *>(user_data);
        self->forceKeyframe();
        std::cout << "🔑 IDR forzado al establecer conexión ICE\n";
    }
}

// ============================================================

bool GStreamerManager::startCapture()
{
    if (is_capturing_)
        return true;
    if (!pipeline_)
    {
        error("Pipeline not initialized");
        return false;
    }

    last_frame_time_ = std::chrono::steady_clock::now();
    watchdog_running_ = true;
    watchdog_thread_ = std::thread(&GStreamerManager::watchdogLoop, this);

    std::cout << "▶️ Starting pipeline...\n";
    if (gst_element_set_state(pipeline_, GST_STATE_PLAYING) == GST_STATE_CHANGE_FAILURE)
    {
        error("Failed to start pipeline");
        return false;
    }

    is_capturing_ = true;
    if (connection_mode_ == ConnectionMode::Cable)
        std::cout << "✅ Streaming via WebRTC (ICE-TCP puerto " << kIceTcpPort << ")\n";
    else
        std::cout << "✅ Streaming via WebRTC (WiFi, ICE-UDP " << kIceWifiUdpPortMin
                  << "-" << kIceWifiUdpPortMax << " + ICE-TCP)\n";
    return true;
}

bool GStreamerManager::restartPipeline(int fd)
{
    std::cout << "🔄 Reiniciando pipeline para nueva sesión WebRTC (fd=" << fd << ")...\n";
    if (!initializePipeline(fd, node_id_))
        return false;
    return startCapture();
}

bool GStreamerManager::restartWebRtcBin()
{
    if (!pipeline_ || !webrtcbin_ || !webrtc_sink_pad_)
        return false;

    GstElement *rtp_out = gst_bin_get_by_name(GST_BIN(pipeline_), "rtp_out");
    if (!rtp_out) return false;
    GstPad *rtp_src = gst_element_get_static_pad(rtp_out, "src");
    gst_object_unref(rtp_out);
    if (!rtp_src) return false;

    // Block the streaming thread on rtp_out's src pad so the encoder does not try
    // to push during the gap where rtp_out has no downstream sink. This avoids the
    // "Failed to push one frame" without pausing the whole pipeline (pausing it
    // broke pipewiresrc's DMABuf caps negotiation).
    struct PadBlock {
        std::mutex mtx;
        std::condition_variable cv;
        bool fired{false};
    } block;

    gulong probe_id = gst_pad_add_probe(
        rtp_src,
        GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
        [](GstPad *, GstPadProbeInfo *, gpointer data) -> GstPadProbeReturn {
            auto *b = static_cast<PadBlock *>(data);
            {
                std::lock_guard<std::mutex> lock(b->mtx);
                b->fired = true;
            }
            b->cv.notify_one();
            return GST_PAD_PROBE_OK; // holds the block until we remove the probe
        },
        &block, nullptr);

    // Wait at most 100 ms for the probe to fire. If the screen is static,
    // pipewiresrc pushes no frames and the probe never fires; the swap is still
    // safe in that case because the encoder is not trying to push either.
    {
        std::unique_lock<std::mutex> lock(block.mtx);
        block.cv.wait_for(lock, std::chrono::milliseconds(100),
                          [&block] { return block.fired; });
    }

    // CRITICAL: unlink rtp_src BEFORE gst_element_set_state(NULL).
    // If it were still linked, the NULL transition would fire FLUSH events that
    // travel upstream through rtp_src - which the probe is blocking - causing a
    // deadlock (the FLUSH needs the STREAM_LOCK the probe holds).
    gst_pad_unlink(rtp_src, webrtc_sink_pad_);
    gst_element_release_request_pad(webrtcbin_, webrtc_sink_pad_);
    gst_object_unref(webrtc_sink_pad_);
    webrtc_sink_pad_ = nullptr;

    gst_element_set_state(webrtcbin_, GST_STATE_NULL); // safe: pad already unlinked
    gst_bin_remove(GST_BIN(pipeline_), webrtcbin_);
    webrtcbin_ = nullptr;

    // Create and configure the new webrtcbin
    webrtcbin_ = gst_element_factory_make("webrtcbin", "webrtcbin");
    if (!webrtcbin_)
    {
        gst_pad_remove_probe(rtp_src, probe_id);
        gst_object_unref(rtp_src);
        return false;
    }

    g_object_set(G_OBJECT(webrtcbin_), "bundle-policy", 3 /* MAX_BUNDLE */, NULL);

    GObject *ice_agent = nullptr;
    g_object_get(G_OBJECT(webrtcbin_), "ice-agent", &ice_agent, NULL);
    if (ice_agent)
    {
        configureIceForMode(ice_agent);
        g_object_unref(ice_agent);
    }

    g_signal_connect(webrtcbin_, "on-ice-candidate", G_CALLBACK(onIceCandidateCb), this);
    g_signal_connect(webrtcbin_, "notify::ice-connection-state", G_CALLBACK(onIceConnectionStateCb), this);

    gst_bin_add(GST_BIN(pipeline_), webrtcbin_);

    webrtc_sink_pad_ = gst_element_request_pad_simple(webrtcbin_, "sink_%u");
    if (!webrtc_sink_pad_ || gst_pad_link(rtp_src, webrtc_sink_pad_) != GST_PAD_LINK_OK)
    {
        std::cerr << "GStreamer Error: Failed to link rtp_out to new webrtcbin\n";
        if (webrtc_sink_pad_)
        {
            gst_element_release_request_pad(webrtcbin_, webrtc_sink_pad_);
            gst_object_unref(webrtc_sink_pad_);
            webrtc_sink_pad_ = nullptr;
        }
        gst_pad_remove_probe(rtp_src, probe_id);
        gst_object_unref(rtp_src);
        return false;
    }

    gst_element_sync_state_with_parent(webrtcbin_);

    // Unblock the streaming thread: the frame held by the probe is now pushed to
    // the new webrtcbin. GStreamer sends the CAPS event before the first buffer,
    // so the negotiation is transparent.
    gst_pad_remove_probe(rtp_src, probe_id);
    gst_object_unref(rtp_src);

    // Extra IDR to make sure the client receives a decodable keyframe.
    // onIceConnectionStateCb will also force another IDR once ICE is connected.
    forceKeyframe();

    std::cout << "🔄 WebRTC bin reemplazado (pipewiresrc sigue corriendo)\n";
    return true;
}

void GStreamerManager::stopCapture()
{
    if (!is_capturing_ || !pipeline_)
        return;
    watchdog_running_ = false;
    if (watchdog_thread_.joinable())
        watchdog_thread_.join();
    gst_element_set_state(pipeline_, GST_STATE_NULL);
    is_capturing_ = false;
    std::cout << "⏹️ Pipeline stopped\n";
}

GstBusSyncReply GStreamerManager::onBusMessage(GstBus *bus, GstMessage *message, gpointer data)
{
    (void)bus;
    GStreamerManager *self = static_cast<GStreamerManager *>(data);

    switch (GST_MESSAGE_TYPE(message))
    {
    case GST_MESSAGE_ERROR:
    {
        GError *err;
        gchar *debug_info;
        gst_message_parse_error(message, &err, &debug_info);
        std::cerr << "GStreamer Error: " << err->message << "\n";
        if (debug_info)
            std::cerr << "   Debug: " << debug_info << "\n";
        g_clear_error(&err);
        g_free(debug_info);
        self->error("GStreamer error occurred");
        break;
    }
    case GST_MESSAGE_WARNING:
    {
        GError *err;
        gchar *debug_info;
        gst_message_parse_warning(message, &err, &debug_info);
        std::cerr << "GStreamer Warning: " << err->message << "\n";
        if (debug_info)
            std::cerr << "   Debug: " << debug_info << "\n";
        g_clear_error(&err);
        g_free(debug_info);
        break;
    }
    case GST_MESSAGE_EOS:
        std::cout << "End of stream\n";
        break;
    case GST_MESSAGE_STATE_CHANGED:
        if (GST_MESSAGE_SRC(message) == GST_OBJECT(self->pipeline_))
        {
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

void GStreamerManager::watchdogLoop()
{
    auto last_log = std::chrono::steady_clock::now() - std::chrono::seconds(10);

    while (watchdog_running_)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~1 frame at 60fps
        if (!is_capturing_)
            continue;

        auto now = std::chrono::steady_clock::now();

        if ((now - last_frame_time_) < kStallThreshold)
            continue;

        // Static screen: pipewiresrc delivers no new frames. This is not an error
        // (WebRTC simply sends nothing until the image changes); we just log it,
        // without touching pipewiresrc's state (that broke caps renegotiation with
        // the new rtph264pay/webrtcbin tail).
        if ((now - last_log) >= std::chrono::milliseconds(1000))
        {
            last_log = now;
            std::cout << "⏸️ Watchdog: stall (pantalla estática)\n";
        }
    }
}

void GStreamerManager::forceKeyframe()
{
    if (!encoder_)
        return;
    // Send an upstream event to the encoder to generate an IDR on the next frame
    GstPad *sink_pad = gst_element_get_static_pad(encoder_, "sink");
    if (sink_pad)
    {
        GstEvent *event = gst_video_event_new_upstream_force_key_unit(
            GST_CLOCK_TIME_NONE, TRUE, 0);
        gst_pad_push_event(sink_pad, event);
        gst_object_unref(sink_pad);
        std::cout << "🔑 IDR forzado tras reanudación de stream\n";
    }
}

void GStreamerManager::cleanup()
{
    if (pipeline_)
    {
        if (webrtc_sink_pad_ && webrtcbin_)
        {
            gst_element_release_request_pad(webrtcbin_, webrtc_sink_pad_);
            gst_object_unref(webrtc_sink_pad_);
        }
        webrtc_sink_pad_ = nullptr;

        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        pipewiresrc_ = queue_main_ = convert_ = nullptr;
        encoder_ = h264parse_ = rtph264pay_ = webrtcbin_ = nullptr;
    }
}

void GStreamerManager::error(const std::string &message)
{
    std::cerr << "GStreamer Error: " << message << "\n";
    is_capturing_ = false;
    if (error_callback_)
        error_callback_(message);
}