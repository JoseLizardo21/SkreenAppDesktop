#include "GStreamerManager.h"
#include <gst/video/video.h>
#define GST_USE_UNSTABLE_API
#include <gst/webrtc/webrtc.h>
#include <gst/sdp/sdp.h>
#include <nice/agent.h>
#include <iostream>
#include <string>

namespace
{
// DIAGNÓSTICO TEMPORAL: cuenta buffers por etapa a lo largo de la sesión, para
// ver si el flujo es continuo (sesión 1) o se detiene tras el primero (sesión 2+).
struct ProbeCounter
{
    const char *label;
    int count;
};
ProbeCounter g_pwsrc_counter{"pipewiresrc src", 0};
ProbeCounter g_h264_counter{"h264parse src", 0};
ProbeCounter g_queue_counter{"queue_main src", 0};
ProbeCounter g_convert_counter{"videoconvert src", 0};
ProbeCounter g_enc_sink_counter{"encoder sink", 0};
ProbeCounter g_enc_src_counter{"encoder src", 0};

GstPadProbeReturn countBuffers(GstPad *, GstPadProbeInfo *, gpointer user_data)
{
    auto *c = static_cast<ProbeCounter *>(user_data);
    c->count++;
    if (c->count <= 5 || c->count % 30 == 0)
        std::cout << "🟢 " << c->label << ": buffer #" << c->count << "\n";
    return GST_PAD_PROBE_OK;
}
} // namespace

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
    videoconvert_ = gst_element_factory_make("videoconvert", "convert");
    h264parse_ = gst_element_factory_make("h264parse", "parser");
    rtph264pay_ = gst_element_factory_make("rtph264pay", "pay");
    webrtcbin_ = gst_element_factory_make("webrtcbin", "webrtcbin");

    // Encoder: HW primero, SW fallback
    struct Candidate
    {
        const char *name;
        const char *label;
    };
    static const Candidate candidates[] = {
        // DIAGNÓSTICO TEMPORAL: VAAPI deshabilitado para probar si el encoder
        // de software también se queda sin producir buffers tras el primero
        // en la segunda sesión.
        // {"vah264enc", "Intel/AMD H.264 VA-API"},
        // {"vaapih264enc", "Intel/AMD H.264 VAAPI"},
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
                         NULL);

            // CBR vía nick de string: evita depender de valores numéricos del
            // enum, que varían entre versiones del plugin gstreamer-vaapi.
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

    if (!pipewiresrc_ || !queue_main_ || !videoconvert_ ||
        !encoder_ || !h264parse_ || !rtph264pay_ || !webrtcbin_)
    {
        error("Failed to create one or more elements");
        if (!encoder_)
            std::cerr << "  ❌ No H.264 encoder available\n";
        if (!webrtcbin_)
            std::cerr << "  ❌ webrtcbin not available\n";
        return false;
    }

    // Queue: leaky=2 (downstream) descarta frames viejos, mantiene el más reciente
    g_object_set(G_OBJECT(queue_main_),
                 "max-size-buffers", 1, "max-size-bytes", 0,
                 "max-size-time", 0, "leaky", 2, NULL);

    // h264parse: SPS/PPS antes de cada IDR
    g_object_set(G_OBJECT(h264parse_),
                 "config-interval", -1,
                 "disable-passthrough", TRUE,
                 NULL);

    // rtph264pay: repite SPS/PPS en cada IDR para que un cliente que se conecta
    // a mitad de stream pueda decodificar desde el primer keyframe
    g_object_set(G_OBJECT(rtph264pay_),
                 "config-interval", -1,
                 "pt", 96,
                 NULL);

    // webrtcbin: un solo m-line de video, sin STUN/TURN (todo es loopback vía adb)
    g_object_set(G_OBJECT(webrtcbin_),
                 "bundle-policy", 3 /* GST_WEBRTC_BUNDLE_POLICY_MAX_BUNDLE */,
                 NULL);

    // Forzar ICE-TCP: adb reverse/forward solo tunelizan TCP, no UDP. Se desactiva
    // la recolección de candidatos UDP y se fija el puerto TCP a kIceTcpPort para
    // poder reenviarlo con un único "adb reverse tcp:9006 tcp:9006".
    GObject *ice_agent = nullptr;
    g_object_get(G_OBJECT(webrtcbin_), "ice-agent", &ice_agent, NULL);
    if (ice_agent)
    {
        g_object_set(ice_agent,
                     "ice-udp", FALSE,
                     "ice-tcp", TRUE,
                     "min-rtp-port", kIceTcpPort,
                     "max-rtp-port", kIceTcpPort,
                     NULL);

        // Restringir la recolección de candidatos a loopback: sin esto, libnice
        // detecta automáticamente todas las interfaces locales (p.ej. Wi-Fi) y
        // ofrece candidatos TCP en esas IPs. Si el cliente las elige, la
        // conexión queda atada a esa red en vez de al túnel adb (USB).
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

        g_object_unref(ice_agent);
    }

    g_signal_connect(webrtcbin_, "on-ice-candidate", G_CALLBACK(onIceCandidateCb), this);

    std::cout << "  ✓ All elements created\n";
    return true;
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

    // Capsfilter post-pipewiresrc: fuerza memoria de sistema (sin DMABuf).
    // En máquinas físicas con GPU, el portal de screencast suele ofrecer
    // memory:DMABuf, que videoconvert (CPU) no puede negociar. Esto fuerza
    // a pipewiresrc a caer a un formato en memoria de sistema (SHM) si el
    // stream lo soporta como alternativa.
    GstElement *src_caps = gst_element_factory_make("capsfilter", "src_caps");
    GstCaps *sys_mem_caps = gst_caps_new_empty_simple("video/x-raw");
    GstCapsFeatures *sys_mem_features = gst_caps_features_new(GST_CAPS_FEATURE_MEMORY_SYSTEM_MEMORY, NULL);
    gst_caps_set_features(sys_mem_caps, 0, sys_mem_features);
    g_object_set(G_OBJECT(src_caps), "caps", sys_mem_caps, NULL);
    gst_caps_unref(sys_mem_caps);

    // DIAGNÓSTICO TEMPORAL: videorate/rate_caps deshabilitado — parece estar
    // bloqueando los buffers tras el primero (ver g_h264_counter).
    // GstElement *videorate = gst_element_factory_make("videorate", "rate");
    // g_object_set(G_OBJECT(videorate), "drop-only", FALSE, "skip-to-first", TRUE, NULL);
    //
    // GstElement *rate_caps = gst_element_factory_make("capsfilter", "rate_caps");
    // GstCaps *rate_caps_val = gst_caps_new_simple("video/x-raw",
    //                                              "framerate", GST_TYPE_FRACTION, 30, 1,
    //                                              NULL);
    // g_object_set(G_OBJECT(rate_caps), "caps", rate_caps_val, NULL);
    // gst_caps_unref(rate_caps_val);

    // Capsfilter pre-encoder: solo para x264enc, fuerza I420+sRGB para evitar tinte verde
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

    // Capsfilter que fuerza byte-stream (Annex-B) en la salida del h264parse
    GstElement *h264out = gst_element_factory_make("capsfilter", "h264_out");
    GstCaps *sink_caps = gst_caps_new_simple("video/x-h264",
                                             "stream-format", G_TYPE_STRING, "byte-stream",
                                             "alignment", G_TYPE_STRING, "au",
                                             NULL);
    g_object_set(G_OBJECT(h264out), "caps", sink_caps, NULL);
    gst_caps_unref(sink_caps);

    // Capsfilter de salida del payloader: caps RTP explícitos para el sink_%u de webrtcbin
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
                     pipewiresrc_, src_caps, queue_main_, videoconvert_,
                     enc_in, encoder_, h264parse_, h264out,
                     rtph264pay_, rtp_out, webrtcbin_,
                     NULL);

    if (!gst_element_link_many(pipewiresrc_, src_caps, queue_main_, videoconvert_,
                               enc_in, encoder_, h264parse_, h264out,
                               rtph264pay_, rtp_out,
                               NULL))
    {
        error("Failed to link pipeline elements");
        return false;
    }

    // rtp_out -> webrtcbin (pad de petición: crea un transceiver de video sendonly)
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
    gst_object_unref(webrtc_sink);

    // Lee width/height reales del stream codificado (para mapear coordenadas de touch)
    GstPad *parse_src = gst_element_get_static_pad(h264parse_, "src");
    gst_pad_add_probe(parse_src, GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM,
                      onCapsProbe, this, NULL);
    gst_object_unref(parse_src);

    // Marca de actividad para el watchdog de stall (forceKeyframe tras reanudar)
    GstPad *pay_sink = gst_element_get_static_pad(rtph264pay_, "sink");
    gst_pad_add_probe(pay_sink, GST_PAD_PROBE_TYPE_BUFFER,
                      onPayloaderBuffer, this, NULL);
    gst_object_unref(pay_sink);

    // DIAGNÓSTICO TEMPORAL: localizar en qué etapa se detienen los buffers
    // en una sesión que se queda en stall
    {
        GstPad *p;
        g_pwsrc_counter.count = 0;
        g_h264_counter.count = 0;
        g_queue_counter.count = 0;
        g_convert_counter.count = 0;
        g_enc_sink_counter.count = 0;
        g_enc_src_counter.count = 0;

        p = gst_element_get_static_pad(pipewiresrc_, "src");
        gst_pad_add_probe(p, GST_PAD_PROBE_TYPE_BUFFER, countBuffers, &g_pwsrc_counter, NULL);
        gst_object_unref(p);

        p = gst_element_get_static_pad(queue_main_, "src");
        gst_pad_add_probe(p, GST_PAD_PROBE_TYPE_BUFFER, countBuffers, &g_queue_counter, NULL);
        gst_object_unref(p);

        p = gst_element_get_static_pad(videoconvert_, "src");
        gst_pad_add_probe(p, GST_PAD_PROBE_TYPE_BUFFER, countBuffers, &g_convert_counter, NULL);
        gst_object_unref(p);

        p = gst_element_get_static_pad(encoder_, "sink");
        gst_pad_add_probe(p, GST_PAD_PROBE_TYPE_BUFFER, countBuffers, &g_enc_sink_counter, NULL);
        gst_object_unref(p);

        p = gst_element_get_static_pad(encoder_, "src");
        gst_pad_add_probe(p, GST_PAD_PROBE_TYPE_BUFFER, countBuffers, &g_enc_src_counter, NULL);
        gst_object_unref(p);

        p = gst_element_get_static_pad(h264parse_, "src");
        gst_pad_add_probe(p, GST_PAD_PROBE_TYPE_BUFFER, countBuffers, &g_h264_counter, NULL);
        gst_object_unref(p);
    }

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

    // Si el pipeline estuvo parado (PipeWire inactivo), forzar IDR inmediato
    // para que el cliente reciba un frame limpio sin artefactos de referencia
    if (was_stalled)
        self->forceKeyframe();

    return GST_PAD_PROBE_OK;
}

// ============================================================
// Señalización WebRTC
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
    std::cout << "✅ Streaming via WebRTC (ICE-TCP puerto " << kIceTcpPort << ")\n";
    return true;
}

bool GStreamerManager::restartPipeline(int fd)
{
    std::cout << "🔄 Reiniciando pipeline para nueva sesión WebRTC (fd=" << fd << ")...\n";
    if (!initializePipeline(fd, node_id_))
        return false;
    return startCapture();
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
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~1 frame a 60fps
        if (!is_capturing_)
            continue;

        auto now = std::chrono::steady_clock::now();

        if ((now - last_frame_time_) < kStallThreshold)
            continue;

        // Pantalla estática: pipewiresrc no entrega frames nuevos. No es un error
        // (WebRTC simplemente no envía nada hasta que cambie la imagen); solo lo
        // registramos, sin tocar el estado de pipewiresrc (eso rompía la
        // renegociación de caps con el nuevo tail rtph264pay/webrtcbin).
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
    // Envía evento upstream al encoder para generar un IDR en el próximo frame
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
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        pipewiresrc_ = queue_main_ = videoconvert_ = nullptr;
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