#include "WebSocketClient.h"
#include <iostream>
#include <unistd.h>
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>
#include <nlohmann/json.hpp>
#include <sys/wait.h>

using json = nlohmann::json;

typedef websocketpp::client<websocketpp::config::asio_client> client;
typedef websocketpp::config::asio_client::message_type::ptr message_ptr;

class WebSocketClientImpl {
public:
    client c;
    websocketpp::connection_hdl hdl;
    bool is_connected = false;
};

WebSocketClient::WebSocketClient(const std::string& server_url)
    : server_url_(server_url), impl_(std::make_unique<WebSocketClientImpl>()) {
}

void WebSocketClient::connect() {
    if (running_.exchange(true)) {
        return;
    }

    std::cout << "[WebSocket] Iniciando conexión a " << server_url_ << "...\n";

    connect_thread_ = std::thread(&WebSocketClient::connectThreadWorker, this);
    message_thread_ = std::thread(&WebSocketClient::messageThreadWorker, this);
}

void WebSocketClient::disconnect() {
    if (!running_.exchange(false)) {
        return;
    }

    std::cout << "[WebSocket] Desconectando...\n";
    connected_ = false;

    try {
        if (impl_->is_connected) {
            impl_->c.close(impl_->hdl, websocketpp::close::status::going_away, "");
        }
        impl_->c.stop();
    } catch (const std::exception& e) {
        std::cerr << "[WebSocket] Error al desconectar: " << e.what() << "\n";
    }

    queue_cv_.notify_all();

    if (connect_thread_.joinable()) {
        connect_thread_.join();
    }

    if (message_thread_.joinable()) {
        message_thread_.join();
    }

    std::cout << "[WebSocket] Desconexión completa.\n";
}

void WebSocketClient::sendMessage(const std::string& message) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        if (message_queue_.size() >= MAX_QUEUE_SIZE) {
            std::cerr << "[WebSocket] Cola llena, descartando mensaje antiguo\n";
            message_queue_.pop();
        }
        message_queue_.push(message);
    }
    queue_cv_.notify_one();
}

void WebSocketClient::connectThreadWorker() {
    try {
        impl_->c.set_access_channels(websocketpp::log::alevel::none);
        impl_->c.clear_access_channels(websocketpp::log::alevel::all);

        impl_->c.init_asio();

        impl_->c.set_open_handler([this](websocketpp::connection_hdl h) {
            impl_->hdl = h;
            impl_->is_connected = true;
            connected_ = true;
            std::cout << "[WebSocket] Conectado al servidor\n";
            std::cout << "[WebSocket] Enviando 'register_cpp'...\n";
            sendMessage("register_cpp");
        });

        impl_->c.set_close_handler([this](websocketpp::connection_hdl) {
            impl_->is_connected = false;
            connected_ = false;
            std::cout << "[WebSocket] Desconectado del servidor\n";
        });

        impl_->c.set_message_handler([this](websocketpp::connection_hdl, message_ptr msg) {
            std::string payload = msg->get_payload();
            handleMessage(payload);
        });

        impl_->c.set_fail_handler([this](websocketpp::connection_hdl h) {
            auto con = impl_->c.get_con_from_hdl(h);
            std::cerr << "[WebSocket] Error de conexión: " << con->get_ec().message() << "\n";
        });

        while (running_) {
            try {
                std::error_code ec;
                auto con = impl_->c.get_connection(server_url_, ec);

                if (ec) {
                    std::cerr << "[WebSocket] Error al parsear URL: " << ec.message() << "\n";
                    usleep(1000000);
                    continue;
                }

                impl_->c.connect(con);
                impl_->c.run();

                // Resetear el cliente para liberar recursos después de la desconexión
                if (running_) {
                    impl_->c.reset();
                    impl_->c.init_asio();
                }
            } catch (const std::exception& e) {
                std::cerr << "[WebSocket] Error: " << e.what() << "\n";
                usleep(1000000);

                // Resetear el cliente también en caso de error
                if (running_) {
                    try {
                        impl_->c.reset();
                        impl_->c.init_asio();
                    } catch (...) {
                        std::cerr << "[WebSocket] Error al resetear cliente\n";
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[WebSocket] Error en thread de conexión: " << e.what() << "\n";
    }

    std::cout << "[WebSocket] Thread de conexión terminado\n";
}

void WebSocketClient::messageThreadWorker() {
    while (running_) {
        std::string message;

        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait_for(lock, std::chrono::milliseconds(100), [this] {
                return !message_queue_.empty() || !running_;
            });

            if (!running_) {
                break;
            }

            if (!message_queue_.empty()) {
                message = message_queue_.front();
                message_queue_.pop();
            }
        }

        if (!message.empty() && impl_->is_connected) {
            try {
                impl_->c.send(impl_->hdl, message, websocketpp::frame::opcode::text);
                std::cout << "[WebSocket] Mensaje enviado: " << message << "\n";
            } catch (const std::exception& e) {
                std::cerr << "[WebSocket] Error al enviar: " << e.what() << "\n";
            }
        }
    }

    std::cout << "[WebSocket] Thread de mensajes terminado\n";
}

void WebSocketClient::handleMessage(const std::string& message) {
    try {
        json j = json::parse(message);
        std::string type = j["type"];

        std::cout << "[WebSocket] Mensaje recibido tipo: " << type << "\n";

        // ========== Manejar mensajes WebRTC ==========
        if (type == "offer" || type == "answer") {
            // Mensaje SDP de WebRTC
            std::string sdp = j["data"];

            std::cout << "[WebRTC] Recibido " << type << " SDP\n";

            if (on_sdp_callback_) {
                on_sdp_callback_(type, sdp);
            }
            return;
        }

        if (type == "candidate") {
            // ICE Candidate de WebRTC
            int mline_index = j.value("mlineIndex", 0);
            std::string candidate = j["data"];

            std::cout << "[WebRTC] Recibido ICE candidate (mline: " << mline_index << ")\n";

            if (on_ice_callback_) {
                on_ice_callback_(mline_index, candidate);
            }
            return;
        }

        // ========== Mensajes existentes (client_info y screen_info) ==========
        if (type == "client_info" || type == "screen_info" || j.contains("device_name")) {
            if (on_transmit_button_callback_) {
                on_transmit_button_callback_(true);
            }
            std::string device_name = j["device_name"];
            int width = j["width"];
            int height = j["height"];

            std::cout << "[WebSocket] Client info: " << device_name
                      << " (" << width << "x" << height << ")\n";

            // Llamar callback de información del cliente
            if (on_client_info_callback_) {
                try {
                    on_client_info_callback_(device_name, width, height);
                } catch (const std::exception& ex) {
                    std::cerr << "[WebSocket] Excepción en callback de cliente: " << ex.what() << "\n";
                }
            }

            // Restart service
            // char instance_name[64];
            // snprintf(instance_name, sizeof(instance_name), "%dx%d", width, height);

            // char cmd[128];
            // snprintf(cmd, sizeof(cmd), "systemctl restart skcreen@%s.service", instance_name);

            // std::cout << "[TCP] Ejecutando: " << cmd << "\n";

            // pid_t pid = fork();
            // if (pid == 0) {
            //     execl("/bin/sh", "sh", "-c", cmd, nullptr);
            //     std::cerr << "[TCP] Error al ejecutar systemctl\n";
            //     _exit(127);
            // } else if (pid > 0) {
            //     int status;
            //     waitpid(pid, &status, 0);
            //     if (WIFEXITED(status)) {
            //         std::cout << "[TCP] systemctl terminó con código: " << WEXITSTATUS(status) << "\n";
            //     } else {
            //         std::cout << "[TCP] systemctl terminó anormalmente\n";
            //     }
            // } else {
            //     std::cerr << "[TCP] Error al hacer fork\n";
            // }
            return;
        }

        // Callback genérico para otros mensajes
        if (on_message_callback_) {
            try {
                on_message_callback_(message);
            } catch (const std::exception& ex) {
                std::cerr << "[WebSocket] Excepción en callback: " << ex.what() << "\n";
            }
        }

    } catch (const json::parse_error& e) {
        std::cerr << "[WebSocket] Error parseando JSON: " << e.what() << "\n";
        std::cerr << "[WebSocket] Mensaje: " << message << "\n";
    } catch (const std::exception& e) {
        std::cerr << "[WebSocket] Error procesando mensaje: " << e.what() << "\n";
    }
}

WebSocketClient::~WebSocketClient() {
    disconnect();
}
