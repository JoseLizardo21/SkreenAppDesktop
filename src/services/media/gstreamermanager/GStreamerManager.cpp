#define GST_USE_UNSTABLE_API
#include "../../websocketclient/WebSocketClient.h"
#include "GStreamerManager.h"
#include <iostream>
#include <cstring>
#include <nlohmann/json.hpp>
#include <gst/sdp/sdp.h>

using json = nlohmann::json;

GStreamerManager::GStreamerManager(WebSocketClient* ws_client)
    : ws_client_(ws_client) {
}

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

    // Cleanup previous pipeline if exists
    if (pipeline_) {
        std::cout << "Cleaning up previous pipeline...\n";
        disableWebRTC();
        stopCapture();
        cleanup();
    }

    fd_ = fd;
    node_id_ = node_id;

    std::cout << "🎬 Initializing GStreamer pipeline...\n";

    if (!createElements()) {
        return false;
    }

    if (!configurePipeWireSource() ||
        !configureFrameRateFilter() ||
        !linkElements() ||
        !setupBusHandler()) {
        cleanup();
        return false;
    }

    std::cout << "Pipeline initialized successfully\n";
    return true;
}

bool GStreamerManager::createElements() {
    std::cout << "  Creating GStreamer elements...\n";

    pipewiresrc_ = gst_element_factory_make("pipewiresrc", "source");
    videorate_ = gst_element_factory_make("videorate", "rate");
    capsfilter_rate_ = gst_element_factory_make("capsfilter", "filter_rate");
    queue_main_ = gst_element_factory_make("queue", "queue_main");
    g_object_set(G_OBJECT(queue_main_),
                 "max-size-buffers", 1,
                 "max-size-bytes", 0,
                 "max-size-time", 0,
                 "leaky", 2,
                 NULL);

    if (!pipewiresrc_ || !videorate_ || !capsfilter_rate_ || !queue_main_) {
        error("Failed to create GStreamer elements");
        return false;
    }

    std::cout << "  ✓ All elements created\n";
    return true;
}

bool GStreamerManager::configurePipeWireSource() {
    std::cout << "  Configuring PipeWire source...\n";

    g_object_set(G_OBJECT(pipewiresrc_),
                 "fd", fd_,
                 "path", g_strdup_printf("%u", node_id_),
                 NULL);

    std::cout << "  ✓ PipeWire configured (fd=" << fd_
              << ", node_id=" << node_id_ << ")\n";
    return true;
}

bool GStreamerManager::configureFrameRateFilter() {
    std::cout << "  Configuring frame rate filter...\n";

    // Solo descartar frames, nunca duplicar (evita buffering interno)
    g_object_set(G_OBJECT(videorate_), "drop-only", TRUE, NULL);

    GstCaps* caps_rate = gst_caps_new_simple("video/x-raw",
                                             "framerate", GST_TYPE_FRACTION, 30, 1,
                                             NULL);
    g_object_set(G_OBJECT(capsfilter_rate_), "caps", caps_rate, NULL);
    gst_caps_unref(caps_rate);

    std::cout << "  ✓ Frame rate limited to 30 fps\n";
    return true;
}

bool GStreamerManager::linkElements() {
    std::cout << "  Linking pipeline elements...\n";

    pipeline_ = gst_pipeline_new("skreenapp-pipeline");


    gst_bin_add_many(GST_BIN(pipeline_),
                     pipewiresrc_,
                     videorate_,
                     capsfilter_rate_,
                     queue_main_,
                     NULL);

    if (!gst_element_link_many(pipewiresrc_,
                               videorate_,
                               capsfilter_rate_,
                               queue_main_,
                               NULL)) {
        error("Failed to link pipeline elements");
        return false;
    }

    std::cout << "  ✓ Pipeline linked\n";
    return true;
}

bool GStreamerManager::setupBusHandler() {
    std::cout << "  Setting up bus handler...\n";

    GstBus* bus = gst_element_get_bus(pipeline_);
    if (!bus) {
        error("Failed to get bus");
        return false;
    }

    gst_bus_set_sync_handler(bus, (GstBusSyncHandler)onBusMessage, this, NULL);
    gst_object_unref(bus);

    std::cout << "  ✓ Bus handler set\n";
    return true;
}

bool GStreamerManager::startCapture() {
    if (is_capturing_) {
        std::cout << "⚠️ Already capturing\n";
        return true;
    }

    if (!pipeline_) {
        error("Pipeline not initialized");
        return false;
    }

    std::cout << "▶️ Starting pipeline...\n";

    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);

    if (ret == GST_STATE_CHANGE_FAILURE) {
        error("Failed to start pipeline");
        return false;
    }

    is_capturing_ = true;
    std::cout << "Pipeline started - Preview active\n";
    return true;
}

void GStreamerManager::stopCapture() {
    if (!is_capturing_ || !pipeline_) {
        return;
    }

    std::cout << "Stopping pipeline...\n";

    gst_element_set_state(pipeline_, GST_STATE_NULL);
    is_capturing_ = false;

    std::cout << "Pipeline stopped\n";
}

GstBusSyncReply GStreamerManager::onBusMessage(GstBus* bus, GstMessage* message, gpointer data) {
    (void)bus;  // Unused
    GStreamerManager* self = static_cast<GStreamerManager*>(data);

    switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_ERROR: {
            GError* err;
            gchar* debug_info;
            gst_message_parse_error(message, &err, &debug_info);
            std::cerr << "GStreamer Error: " << err->message << "\n";
            if (debug_info) {
                std::cerr << "   Debug: " << debug_info << "\n";
            }
            g_clear_error(&err);
            g_free(debug_info);
            self->error("GStreamer error occurred");
            break;
        }
        case GST_MESSAGE_EOS:
            std::cout << "End of stream\n";
            break;
        case GST_MESSAGE_STATE_CHANGED: {
            if (GST_MESSAGE_SRC(message) == GST_OBJECT(self->pipeline_)) {
                GstState old_state, new_state;
                gst_message_parse_state_changed(message, &old_state, &new_state, NULL);
                std::cout << "Pipeline: "
                          << gst_element_state_get_name(old_state) << " -> "
                          << gst_element_state_get_name(new_state) << "\n";
            }
            break;
        }
        default:
            break;
    }

    return GST_BUS_PASS;
}

void GStreamerManager::cleanup() {
    if (pipeline_) {
        gst_object_unref(pipeline_);
        pipeline_ = nullptr;
        pipewiresrc_ = nullptr;
        videorate_ = nullptr;
        capsfilter_rate_ = nullptr;
        queue_main_ = nullptr;
    }
}

void GStreamerManager::error(const std::string& message) {
    std::cerr << "GStreamer Error: " << message << "\n";
    is_capturing_ = false;
    if (error_callback_) {
        error_callback_(message);
    }
}

bool GStreamerManager::enableWebRTC(const std::string& signaling_server) {
    if (webrtc_enabled_) {
        std::cout << "⚠️ WebRTC ya está habilitado\n";
        return true;
    }

    signaling_server_ = signaling_server;

    std::cout << "🌐 Habilitando WebRTC...\n";

    if (!createWebRTCElements()) {
        error("Failed to create WebRTC elements");
        return false;
    }

    if (!linkWebRTCBranch()) {
        error("Failed to link WebRTC branch");
        return false;
    }

    // Configurar callbacks DESPUÉS de enlazar todo
    setupWebRTCCallbacks();

    // Conectar al servidor de señalización DESPUÉS de configurar callbacks
    connectToSignalingServer();

    webrtc_enabled_ = true;
    std::cout << "✅ WebRTC habilitado\n";

    return true;
}

void GStreamerManager::disableWebRTC() {
    if (!webrtc_enabled_) {
        return;
    }
    
    std::cout << "🔌 Deshabilitando WebRTC...\n";
    
    if (webrtcbin_) {
        gst_element_set_state(webrtcbin_, GST_STATE_NULL);
    }
    
    webrtc_enabled_ = false;
    // No reseteamos ws_client_ porque es un puntero externo, no lo poseemos

    std::cout << "✅ WebRTC deshabilitado\n";
}

bool GStreamerManager::createWebRTCElements() {
    std::cout << "  Creando elementos WebRTC...\n";
    
    // Elementos base
    videoscale_webrtc_ = gst_element_factory_make("videoscale", "scale_webrtc");
    capsfilter_webrtc_ = gst_element_factory_make("capsfilter", "caps_webrtc");
    GstCaps* caps_webrtc = gst_caps_new_simple("video/x-raw",
                                               "width",  G_TYPE_INT, 1280,
                                               "height", G_TYPE_INT, 720,
                                               NULL);
    g_object_set(G_OBJECT(capsfilter_webrtc_), "caps", caps_webrtc, NULL);
    gst_caps_unref(caps_webrtc);

    queue_webrtc_ = gst_element_factory_make("queue", "queue_webrtc");
    g_object_set(G_OBJECT(queue_webrtc_),
                 "max-size-buffers", 1,
                 "max-size-bytes", 0,
                 "max-size-time", 0,
                 "leaky", 2,                  // downstream: descarta frames viejos
                 NULL);
    videoconvert_webrtc_ = gst_element_factory_make("videoconvert", "convert_webrtc");
    webrtcbin_ = gst_element_factory_make("webrtcbin", "webrtc");
    
    // ========== SELECCIÓN DE ENCODER: HW primero, SW fallback ==========
    struct EncoderCandidate {
        const char* name;
        const char* payloader;
        const char* label;
    };
    static const EncoderCandidate candidates[] = {
        {"vah264enc",    "rtph264pay", "Intel/AMD H.264 VA-API (nuevo)"},
        {"vaapih264enc", "rtph264pay", "Intel/AMD H.264 VAAPI"},
        {"nvh264enc",    "rtph264pay", "NVIDIA H.264 NVENC"},
        {"vavp9lpenc",   "rtpvp9pay",  "Intel VP9 VA-API (hardware)"},
        {"x264enc",      "rtph264pay", "H.264 x264 software"},
        {"openh264enc",  "rtph264pay", "H.264 OpenH264 software"},
        {"vp8enc",       "rtpvp8pay",  "VP8 software"},
        {nullptr, nullptr, nullptr}
    };

    for (int i = 0; candidates[i].name && !x264enc_; i++) {
        x264enc_ = gst_element_factory_make(candidates[i].name, "encoder");
        if (!x264enc_) continue;

        const std::string enc_name = candidates[i].name;
        std::cout << "  ✓ Encoder: " << candidates[i].label << "\n";

        if (enc_name == "vah264enc") {
            g_object_set(G_OBJECT(x264enc_),
                         "bitrate", 2000,
                         "key-int-max", 60,
                         "target-usage", 7,
                         NULL);
        } else if (enc_name == "vaapih264enc") {
            g_object_set(G_OBJECT(x264enc_),
                         "bitrate", 2000,
                         "keyframe-period", 60,
                         "quality-level", 7,
                         NULL);
        } else if (enc_name == "nvh264enc") {
            g_object_set(G_OBJECT(x264enc_),
                         "bitrate", 2000,
                         "preset", 6,
                         "rc-mode", 2,
                         "zerolatency", TRUE,
                         NULL);
        } else if (enc_name == "vavp9lpenc") {
            g_object_set(G_OBJECT(x264enc_),
                         "rate-control", 16,  // CQP: sin throttling de bitrate
                         "qp", 35,            // calidad (0-255, menor=mejor)
                         "key-int-max", 60,
                         "target-usage", 7,
                         NULL);
        } else if (enc_name == "x264enc") {
            g_object_set(G_OBJECT(x264enc_),
                         "tune", 0x00000004,
                         "speed-preset", 1,
                         "bitrate", 2000,
                         "key-int-max", 60,
                         "threads", 4,
                         "sliced-threads", TRUE,
                         "vbv-buf-capacity", 400,
                         "byte-stream", TRUE,
                         "aud", FALSE,
                         "cabac", FALSE,
                         "dct8x8", FALSE,
                         "bframes", 0,
                         NULL);
        } else if (enc_name == "openh264enc") {
            g_object_set(G_OBJECT(x264enc_),
                         "bitrate", 2000000,
                         "complexity", 0,
                         "rate-control", 1,
                         NULL);
        } else {
            // vp8enc
            g_object_set(G_OBJECT(x264enc_),
                         "deadline", 1,
                         "target-bitrate", 2000000,
                         "cpu-used", 4,
                         "keyframe-max-dist", 30,
                         NULL);
        }

        rtph264pay_ = gst_element_factory_make(candidates[i].payloader, "payloader");
        if (!rtph264pay_) {
            gst_object_unref(x264enc_);
            x264enc_ = nullptr;
            continue;
        }

        // Configurar payloader según tipo
        if (enc_name == "vp8enc") {
            g_object_set(G_OBJECT(rtph264pay_), "pt", 96, NULL);
        } else if (enc_name == "vavp9lpenc") {
            g_object_set(G_OBJECT(rtph264pay_), "pt", 97, NULL);
        } else {
            // H.264
            g_object_set(G_OBJECT(rtph264pay_),
                         "config-interval", -1,
                         "pt", 96,
                         "mtu", 1400,
                         "aggregate-mode", 1,
                         NULL);
        }
    }

    if (!x264enc_) {
        error("No hay encoder de video disponible");
        return false;
    }
    
    // ========== VERIFICAR TODOS LOS ELEMENTOS ==========
    if (!queue_webrtc_ || !videoscale_webrtc_ || !capsfilter_webrtc_ ||
        !videoconvert_webrtc_ || !x264enc_ || !rtph264pay_ || !webrtcbin_) {
        error("Failed to create one or more WebRTC elements");
        if (!queue_webrtc_)        std::cerr << "  ❌ queue_webrtc\n";
        if (!videoscale_webrtc_)   std::cerr << "  ❌ videoscale_webrtc\n";
        if (!capsfilter_webrtc_)   std::cerr << "  ❌ capsfilter_webrtc\n";
        if (!videoconvert_webrtc_) std::cerr << "  ❌ videoconvert_webrtc\n";
        if (!x264enc_)             std::cerr << "  ❌ encoder\n";
        if (!rtph264pay_)          std::cerr << "  ❌ payloader\n";
        if (!webrtcbin_)           std::cerr << "  ❌ webrtcbin\n";
        return false;
    }
    
    // Configurar WebRTC
    g_object_set(G_OBJECT(webrtcbin_),
                 "bundle-policy", 3,  // max-bundle
                 "stun-server", "stun://stun.l.google.com:19302",
                 NULL);
    
    std::cout << "  ✓ Elementos WebRTC creados\n";
    return true;
}

bool GStreamerManager::linkWebRTCBranch() {
    if (!pipeline_) {
        error("Pipeline not initialized");
        return false;
    }

    std::cout << "  Enlazando WebRTC...\n";

    gst_bin_add_many(GST_BIN(pipeline_),
                     queue_webrtc_,
                     videoscale_webrtc_,
                     capsfilter_webrtc_,
                     videoconvert_webrtc_,
                     x264enc_,
                     rtph264pay_,
                     webrtcbin_,
                     NULL);

    // queue_main → queue_webrtc
    if (!gst_element_link(queue_main_, queue_webrtc_)) {
        error("Failed to link queue_main to queue_webrtc");
        return false;
    }

    // queue → scale → capsfilter(1280x720) → convert → encoder → payloader
    if (!gst_element_link_many(queue_webrtc_,
                               videoscale_webrtc_,
                               capsfilter_webrtc_,
                               videoconvert_webrtc_,
                               x264enc_,
                               rtph264pay_,
                               NULL)) {
        error("Failed to link WebRTC chain");
        return false;
    }

    // payloader → webrtcbin
    GstPad* pay_src = gst_element_get_static_pad(rtph264pay_, "src");
    GstPad* webrtc_sink = gst_element_request_pad_simple(webrtcbin_, "sink_%u");

    if (!pay_src || !webrtc_sink) {
        error("Failed to get pads for webrtc");
        return false;
    }

    if (gst_pad_link(pay_src, webrtc_sink) != GST_PAD_LINK_OK) {
        error("Failed to link payloader to webrtcbin");
        return false;
    }

    gst_object_unref(pay_src);
    gst_object_unref(webrtc_sink);

    std::cout << "  ✓ WebRTC enlazado\n";
    return true;
}


void GStreamerManager::setupWebRTCCallbacks() {
    std::cout << "  Configurando callbacks WebRTC...\n";

    if (!webrtcbin_) {
        std::cerr << "❌ Error: webrtcbin_ es nullptr\n";
        return;
    }

    // Callback cuando se necesita negociación
    g_signal_connect(webrtcbin_, "on-negotiation-needed",
                     G_CALLBACK(onNegotiationNeeded), this);

    // Callback para ICE candidates
    g_signal_connect(webrtcbin_, "on-ice-candidate",
                     G_CALLBACK(onIceCandidate), this);

    std::cout << "  ✓ Callbacks WebRTC configurados\n";
}

void GStreamerManager::onNegotiationNeeded(GstElement* webrtc, gpointer user_data) {
    GStreamerManager* self = static_cast<GStreamerManager*>(user_data);

    if (!self) {
        std::cerr << "❌ Error: self es nullptr en onNegotiationNeeded\n";
        return;
    }

    std::cout << "🤝 Negotiation needed - Creating offer...\n";

    // Verificar que el WebSocket esté conectado antes de crear la oferta
    if (!self->ws_client_ || !self->ws_client_->isConnected()) {
        std::cout << "⚠️ WebSocket no conectado aún, retrasando negociación...\n";
        return;
    }

    // Crear promise para la oferta
    GstPromise* promise = gst_promise_new_with_change_func(onOfferCreated, self, NULL);

    // Crear oferta SDP
    g_signal_emit_by_name(webrtc, "create-offer", NULL, promise);
}

void GStreamerManager::onOfferCreated(GstPromise* promise, gpointer user_data) {
    GStreamerManager* self = static_cast<GStreamerManager*>(user_data);

    if (!self || !promise) {
        std::cerr << "❌ Error: puntero nulo en onOfferCreated\n";
        if (promise) gst_promise_unref(promise);
        return;
    }

    const GstStructure* reply = gst_promise_get_reply(promise);
    if (!reply) {
        std::cerr << "❌ Error: no se pudo obtener respuesta de promise\n";
        gst_promise_unref(promise);
        return;
    }

    GstWebRTCSessionDescription* offer = NULL;
    gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);

    if (!offer) {
        std::cerr << "❌ Error: no se pudo crear la oferta SDP\n";
        gst_promise_unref(promise);
        return;
    }

    // Establecer oferta local
    g_signal_emit_by_name(self->webrtcbin_, "set-local-description", offer, NULL);

    // Enviar oferta al servidor de señalización
    gchar* sdp_string = gst_sdp_message_as_text(offer->sdp);
    self->sendSDP("offer", sdp_string);
    g_free(sdp_string);

    gst_webrtc_session_description_free(offer);
    gst_promise_unref(promise);

    std::cout << "📤 Offer sent to signaling server\n";
}

void GStreamerManager::onIceCandidate(GstElement* /* webrtc */, guint mlineindex,
                                      gchar* candidate, gpointer user_data) {
    GStreamerManager* self = static_cast<GStreamerManager*>(user_data);

    if (!self || !candidate) {
        std::cerr << "❌ Error: puntero nulo en onIceCandidate\n";
        return;
    }

    std::cout << "🧊 ICE candidate: " << candidate << "\n";
    self->sendICECandidate(mlineindex, candidate);
}

void GStreamerManager::connectToSignalingServer() {
    if (!ws_client_) {
        std::cerr << "❌ WebSocket client no proporcionado\n";
        return;
    }

    std::cout << "Configurando callbacks para WebRTC en WebSocket existente\n";

    // ========== Configurar callbacks WebRTC ==========

    // Callback para SDP (offer/answer)
    ws_client_->setOnSDPCallback([this](const std::string& type, const std::string& sdp) {
        std::cout << "📥 Callback SDP activado: " << type << "\n";
        handleRemoteSDP(type, sdp);
    });

    // Callback para ICE candidates
    ws_client_->setOnICECandidateCallback([this](int mline_index, const std::string& candidate) {
        std::cout << "🧊 Callback ICE activado (mline: " << mline_index << ")\n";
        handleRemoteICE(mline_index, candidate);
    });

    std::cout << "✅ Callbacks WebRTC configurados en WebSocket existente\n";
    std::cout << "⏳ Esperando señal 'on-negotiation-needed' del pipeline...\n";
}

void GStreamerManager::sendSDP(const std::string& type, const std::string& sdp) {
    if (!ws_client_ || !ws_client_->isConnected()) {
        std::cerr << "❌ WebSocket no conectado\n";
        return;
    }

    nlohmann::json j;
    j["type"] = type;
    j["data"] = sdp;

    std::string message = j.dump();
    ws_client_->sendMessage(message);

    std::cout << "📤 Enviado " << type << " SDP al servidor\n";
}

void GStreamerManager::sendICECandidate(guint mline_index, const std::string& candidate) {
    if (!ws_client_ || !ws_client_->isConnected()) {
        std::cerr << "❌ WebSocket no conectado\n";
        return;
    }

    nlohmann::json j;
    j["type"] = "candidate";
    j["mlineIndex"] = mline_index;
    j["data"] = candidate;

    std::string message = j.dump();
    ws_client_->sendMessage(message);

    std::cout << "📤 Enviado ICE candidate (mline: " << mline_index << ")\n";
}

void GStreamerManager::handleRemoteSDP(const std::string& type, const std::string& sdp) {
    std::cout << "📥 Procesando remote " << type << " SDP...\n";

    if (!webrtcbin_) {
        std::cerr << "❌ webrtcbin no inicializado\n";
        return;
    }
    
    GstSDPMessage* sdp_msg;
    gst_sdp_message_new(&sdp_msg);
    gst_sdp_message_parse_buffer((guint8*)sdp.c_str(), sdp.length(), sdp_msg);

    GstWebRTCSDPType desc_type =
        (type == "offer") ? GST_WEBRTC_SDP_TYPE_OFFER : GST_WEBRTC_SDP_TYPE_ANSWER;

    GstWebRTCSessionDescription* remote_desc =
        gst_webrtc_session_description_new(desc_type, sdp_msg);
    
    // Establecer descripción remota
    GstPromise* promise = gst_promise_new();
    g_signal_emit_by_name(webrtcbin_, "set-remote-description", remote_desc, promise);
    gst_promise_interrupt(promise);
    gst_promise_unref(promise);
    
    gst_webrtc_session_description_free(remote_desc);
    
    std::cout << "✅ Remote SDP establecido\n";
    
    // Si recibimos una oferta, crear respuesta automáticamente
    if (type == "offer") {
        std::cout << "📝 Creando answer...\n";
        
        promise = gst_promise_new_with_change_func(
            [](GstPromise* promise, gpointer user_data) {
                GStreamerManager* self = static_cast<GStreamerManager*>(user_data);
                
                const GstStructure* reply = gst_promise_get_reply(promise);
                GstWebRTCSessionDescription* answer = NULL;
                
                gst_structure_get(reply, "answer", 
                                GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &answer, NULL);
                
                // Establecer answer local
                g_signal_emit_by_name(self->webrtcbin_, "set-local-description", 
                                     answer, NULL);
                
                // Enviar answer al peer remoto
                gchar* sdp_string = gst_sdp_message_as_text(answer->sdp);
                self->sendSDP("answer", sdp_string);
                g_free(sdp_string);
                
                gst_webrtc_session_description_free(answer);
                
                // std::cout << "✅ Answer creado y enviado\n";
            }, 
            this, NULL
        );
        
        g_signal_emit_by_name(webrtcbin_, "create-answer", NULL, promise);
        gst_promise_unref(promise);
    }
}

void GStreamerManager::handleRemoteICE(guint mline_index, const std::string& candidate) {
    std::cout << "🧊 Añadiendo remote ICE candidate (mline: " << mline_index << ")\n";
    
    if (!webrtcbin_) {
        std::cerr << "❌ webrtcbin no inicializado\n";
        return;
    }
    
    g_signal_emit_by_name(webrtcbin_, "add-ice-candidate", mline_index, candidate.c_str());
    
    std::cout << "✅ ICE candidate añadido\n";
}