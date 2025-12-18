#define GST_USE_UNSTABLE_API
#include "GStreamerManager.h"
#include <iostream>
#include <cstring>
#include <nlohmann/json.hpp>
#include <gst/sdp/sdp.h>

using json = nlohmann::json;

GStreamerManager::GStreamerManager() = default;

GStreamerManager::~GStreamerManager() {
    stopCapture();
    cleanup();
}

void GStreamerManager::initializeGStreamer() {
    gst_init(nullptr, nullptr);
}

bool GStreamerManager::initializePipeline(int fd, uint32_t node_id, int width, int height) {
    if (fd < 0 || node_id == 0) {
        error("Invalid file descriptor or node ID");
        return false;
    }

    fd_ = fd;
    node_id_ = node_id;
    frame_width_ = width;
    frame_height_ = height;

    std::cout << "🎬 Initializing GStreamer pipeline...\n";

    if (!createElements()) {
        return false;
    }

    if (!configurePipeWireSource() ||
        !configureFrameRateFilter() ||
        !configureScalingFilter() ||
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
    videoconvert1_ = gst_element_factory_make("videoconvert", "convert1");
    videorate_ = gst_element_factory_make("videorate", "rate");
    capsfilter_rate_ = gst_element_factory_make("capsfilter", "filter_rate");
    videoscale_ = gst_element_factory_make("videoscale", "scale");
    capsfilter_scale_ = gst_element_factory_make("capsfilter", "filter_scale");
    videoconvert2_ = gst_element_factory_make("videoconvert", "convert2");

    if (!pipewiresrc_ || !videoconvert1_ || !videorate_ || !capsfilter_rate_ ||
        !videoscale_ || !capsfilter_scale_ || !videoconvert2_) {
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

    GstCaps* caps = gst_caps_new_simple("video/x-raw",
                                        "framerate", GST_TYPE_FRACTION, 30, 1,
                                        NULL);
    g_object_set(G_OBJECT(capsfilter_rate_), "caps", caps, NULL);
    gst_caps_unref(caps);

    std::cout << "  ✓ Frame rate limited to 30 fps\n";
    return true;
}

bool GStreamerManager::configureScalingFilter() {
    std::cout << "  Configuring scaling filter...\n";

    GstCaps* caps = gst_caps_new_simple("video/x-raw",
                                        "width", G_TYPE_INT, frame_width_,
                                        "height", G_TYPE_INT, frame_height_,
                                        NULL);
    g_object_set(G_OBJECT(capsfilter_scale_), "caps", caps, NULL);
    gst_caps_unref(caps);

    std::cout << "  ✓ Scaled to " << frame_width_ << "x" << frame_height_ << "\n";
    return true;
}


bool GStreamerManager::linkElements() {
    std::cout << "  Linking pipeline elements...\n";

    pipeline_ = gst_pipeline_new("skreenapp-pipeline");

    gst_bin_add_many(GST_BIN(pipeline_),
                     pipewiresrc_,
                     videoconvert1_,
                     videorate_,
                     capsfilter_rate_,
                     videoscale_,
                     capsfilter_scale_,
                     videoconvert2_,
                     NULL);

    if (!gst_element_link_many(pipewiresrc_,
                               videoconvert1_,
                               videorate_,
                               capsfilter_rate_,
                               videoscale_,
                               capsfilter_scale_,
                               videoconvert2_,
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
        videoconvert1_ = nullptr;
        videorate_ = nullptr;
        capsfilter_rate_ = nullptr;
        videoscale_ = nullptr;
        capsfilter_scale_ = nullptr;
        videoconvert2_ = nullptr;
    }
}

void GStreamerManager::error(const std::string& message) {
    std::cerr << "GStreamer Error: " << message << "\n";
    is_capturing_ = false;
    if (error_callback_) {
        error_callback_(message);
    }
}

bool GStreamerManager::reconfigureResolution(int width, int height) {
    if (!pipeline_ || !capsfilter_scale_) {
        std::cerr << "Cannot reconfigure: pipeline or capsfilter not initialized\n";
        return false;
    }

    if (width <= 0 || height <= 0) {
        std::cerr << "Invalid resolution: " << width << "x" << height << "\n";
        return false;
    }

    // Pause the pipeline
    GstStateChangeReturn ret = gst_element_set_state(pipeline_, GST_STATE_PAUSED);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Failed to pause pipeline\n";
        return false;
    }

    // Wait for state change to complete
    gst_element_get_state(pipeline_, nullptr, nullptr, 1 * GST_SECOND);

    // Update stored dimensions
    frame_width_ = width;
    frame_height_ = height;

    // Reconfigure the scaling filter caps
    GstCaps* caps = gst_caps_new_simple("video/x-raw",
                                        "width", G_TYPE_INT, frame_width_,
                                        "height", G_TYPE_INT, frame_height_,
                                        NULL);
    g_object_set(G_OBJECT(capsfilter_scale_), "caps", caps, NULL);
    gst_caps_unref(caps);

    std::cout << "  ✓ Reconfigured to " << frame_width_ << "x" << frame_height_ << "\n";

    // Resume the pipeline
    ret = gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    if (ret == GST_STATE_CHANGE_FAILURE) {
        std::cerr << "Failed to resume pipeline\n";
        return false;
    }

    std::cout << "🎬 Pipeline reconfigured with resolution: " << width << "x" << height << "\n";
    return true;
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
    
    setupWebRTCCallbacks();
    connectToSignalingServer();

    webrtc_enabled_ = true;
    std::cout << "✅ WebRTC habilitado\n";

    // Forzar negociación después de un pequeño delay para asegurar que todo esté listo
    // La señal "on-negotiation-needed" debería dispararse automáticamente,
    // pero lo forzamos para asegurar que la oferta se cree
    std::cout << "  Esperando a que WebSocket se conecte...\n";

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
    ws_client_.reset();
    
    std::cout << "✅ WebRTC deshabilitado\n";
}

bool GStreamerManager::createWebRTCElements() {
    std::cout << "  Creando elementos WebRTC...\n";
    
    tee_ = gst_element_factory_make("tee", "tee");
    queue_webrtc_ = gst_element_factory_make("queue", "queue_webrtc");
    videoconvert_webrtc_ = gst_element_factory_make("videoconvert", "convert_webrtc");
    x264enc_ = gst_element_factory_make("x264enc", "encoder");
    rtph264pay_ = gst_element_factory_make("rtph264pay", "payloader");
    webrtcbin_ = gst_element_factory_make("webrtcbin", "webrtc");
    
    // Configurar encoder para baja latencia
    g_object_set(G_OBJECT(x264enc_),
                 "tune", 0x00000004,  // zerolatency
                 "speed-preset", 1,   // ultrafast
                 "bitrate", 2000,     // 2 Mbps
                 "key-int-max", 30,   // Keyframe cada 30 frames
                 NULL);
    
    // Configurar RTP payloader
    g_object_set(G_OBJECT(rtph264pay_),
                 "config-interval", 1,  // Enviar SPS/PPS con cada keyframe
                 "pt", 96,              // Payload type
                 NULL);
    
    // Configurar WebRTC
    g_object_set(G_OBJECT(webrtcbin_),
                 "bundle-policy", 3,  // max-bundle
                 "stun-server", "stun://stun.l.google.com:19302",
                 NULL);
    
    std::cout << "  ✓ Elementos WebRTC creados\n";
    return true;
}

bool GStreamerManager::linkWebRTCBranch() {
    if (!pipeline_ || !tee_) {
        return false;
    }

    std::cout << "  Enlazando rama WebRTC...\n";

    // Pausar el pipeline para modificarlo de forma segura
    GstState current_state, pending_state;
    gst_element_get_state(pipeline_, &current_state, &pending_state, 0);

    if (current_state == GST_STATE_PLAYING) {
        std::cout << "  Pausando pipeline para modificación...\n";
        gst_element_set_state(pipeline_, GST_STATE_PAUSED);
        gst_element_get_state(pipeline_, NULL, NULL, GST_CLOCK_TIME_NONE);
    }

    // Añadir elementos al pipeline
    gst_bin_add_many(GST_BIN(pipeline_),
                     tee_,
                     queue_webrtc_,
                     videoconvert_webrtc_,
                     x264enc_,
                     rtph264pay_,
                     webrtcbin_,
                     NULL);

    // Nueva topología:
    // ... -> videoconvert2_ -> tee
    //                          tee -> queue_preview -> appsink (preview local)
    //                          tee -> queue_webrtc -> videoconvert_webrtc -> x264enc -> rtph264pay -> webrtcbin

    if (!gst_element_link(videoconvert2_, tee_)) {
        error("Failed to link videoconvert2 to tee");
        return false;
    }

    // Rama 2: WebRTC
    if (!gst_element_link_many(tee_, queue_webrtc_, videoconvert_webrtc_,
                               x264enc_, rtph264pay_, NULL)) {
        error("Failed to link WebRTC encoding chain");
        return false;
    }

    // Conectar RTP payloader a webrtcbin
    GstPad* payloader_src = gst_element_get_static_pad(rtph264pay_, "src");
    GstPad* webrtc_sink = gst_element_request_pad_simple(webrtcbin_, "sink_%u");

    if (gst_pad_link(payloader_src, webrtc_sink) != GST_PAD_LINK_OK) {
        error("Failed to link payloader to webrtcbin");
        gst_object_unref(payloader_src);
        gst_object_unref(webrtc_sink);
        return false;
    }

    gst_object_unref(payloader_src);
    gst_object_unref(webrtc_sink);

    // Sincronizar el estado de los nuevos elementos con el pipeline
    gst_element_sync_state_with_parent(tee_);
    gst_element_sync_state_with_parent(queue_webrtc_);
    gst_element_sync_state_with_parent(videoconvert_webrtc_);
    gst_element_sync_state_with_parent(x264enc_);
    gst_element_sync_state_with_parent(rtph264pay_);
    gst_element_sync_state_with_parent(webrtcbin_);

    // Reanudar el pipeline
    if (current_state == GST_STATE_PLAYING) {
        std::cout << "  Reanudando pipeline...\n";
        gst_element_set_state(pipeline_, GST_STATE_PLAYING);
    }

    std::cout << "  ✓ Rama WebRTC enlazada\n";
    return true;
}

void GStreamerManager::setupWebRTCCallbacks() {
    std::cout << "  Configurando callbacks WebRTC...\n";

    // Callback cuando se necesita negociación
    g_signal_connect(webrtcbin_, "on-negotiation-needed",
                     G_CALLBACK(onNegotiationNeeded), this);

    // Callback para ICE candidates
    g_signal_connect(webrtcbin_, "on-ice-candidate",
                     G_CALLBACK(onIceCandidate), this);

    // Crear un transceiver de video para disparar la negociación
    // Esto asegura que webrtcbin sepa que queremos enviar video
    GstWebRTCRTPTransceiverDirection direction = GST_WEBRTC_RTP_TRANSCEIVER_DIRECTION_SENDONLY;
    g_signal_emit_by_name(webrtcbin_, "add-transceiver", direction, NULL);

    std::cout << "  ✓ Callbacks WebRTC configurados\n";
    std::cout << "  ✓ Transceiver de video añadido\n";
}

void GStreamerManager::onNegotiationNeeded(GstElement* webrtc, gpointer user_data) {
    GStreamerManager* self = static_cast<GStreamerManager*>(user_data);
    
    std::cout << "🤝 Negotiation needed - Creating offer...\n";
    
    // Crear promise para la oferta
    GstPromise* promise = gst_promise_new_with_change_func(onOfferCreated, self, NULL);
    
    // Crear oferta SDP
    g_signal_emit_by_name(webrtc, "create-offer", NULL, promise);
}

void GStreamerManager::onOfferCreated(GstPromise* promise, gpointer user_data) {
    GStreamerManager* self = static_cast<GStreamerManager*>(user_data);
    
    const GstStructure* reply = gst_promise_get_reply(promise);
    GstWebRTCSessionDescription* offer = NULL;
    
    gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, NULL);
    
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

    std::cout << "🧊 ICE candidate: " << candidate << "\n";
    self->sendICECandidate(mlineindex, candidate);
}

void GStreamerManager::connectToSignalingServer() {
    // std::cout << "🔌 Connecting to signaling server: " << signaling_server_ << "\n";
    
    // ws_client_ = std::make_shared<WebSocketClient>(signaling_server_);
    
    // // ========== Configurar callbacks WebRTC ==========
    
    // // Callback para SDP (offer/answer)
    // ws_client_->setOnSDPCallback([this](const std::string& type, const std::string& sdp) {
    //     std::cout << "📥 Callback SDP activado: " << type << "\n";
    //     handleRemoteSDP(type, sdp);
    // });
    
    // // Callback para ICE candidates
    // ws_client_->setOnICECandidateCallback([this](int mline_index, const std::string& candidate) {
    //     std::cout << "🧊 Callback ICE activado (mline: " << mline_index << ")\n";
    //     handleRemoteICE(mline_index, candidate);
    // });
    
    // // Callback opcional para mensajes generales
    // ws_client_->setOnMessageCallback([](const std::string& msg) {
    //     std::cout << "📨 Mensaje WebSocket raw: " << msg << "\n";
    // });
    
    // // Conectar
    // ws_client_->connect();
    
    // std::cout << "✅ WebSocket client configurado\n";
}

void GStreamerManager::sendSDP(const std::string& type, const std::string& sdp) {
    // if (!ws_client_ || !ws_client_->isConnected()) {
    //     std::cerr << "❌ WebSocket no conectado\n";
    //     return;
    // }
    
    // nlohmann::json j;
    // j["type"] = type;
    // j["data"] = sdp;
    
    // std::string message = j.dump();
    // ws_client_->sendMessage(message);
    
    // std::cout << "📤 Enviado " << type << " SDP al servidor\n";
}

void GStreamerManager::sendICECandidate(guint mline_index, const std::string& candidate) {
    // if (!ws_client_ || !ws_client_->isConnected()) {
    //     std::cerr << "❌ WebSocket no conectado\n";
    //     return;
    // }
    
    // nlohmann::json j;
    // j["type"] = "candidate";
    // j["mlineIndex"] = mline_index;
    // j["data"] = candidate;
    
    // std::string message = j.dump();
    // ws_client_->sendMessage(message);
    
    // std::cout << "📤 Enviado ICE candidate (mline: " << mline_index << ")\n";
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
                
                std::cout << "✅ Answer creado y enviado\n";
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