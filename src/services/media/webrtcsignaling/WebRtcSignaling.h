#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <functional>

class WebRtcSignaling {
public:
    using MessageCallback = std::function<void(const std::string& json_line)>;
    using ConnectedCallback = std::function<void()>;

    WebRtcSignaling();
    ~WebRtcSignaling();

    void start();
    void stop();

    // message debe incluir el '\n' final (delimitador de mensaje)
    void send(const std::string& message);

    // Se invoca por cada línea JSON recibida del cliente (offer/answer/ice)
    void setOnMessage(MessageCallback callback) { on_message_ = callback; }
    // Se invoca cuando el cliente conecta, para disparar la oferta WebRTC
    void setOnClientConnected(ConnectedCallback callback) { on_client_connected_ = callback; }
    // Se invoca cuando el cliente se desconecta (mientras el servidor sigue corriendo),
    // para refrescar el pipeline antes de aceptar la próxima conexión
    void setOnClientDisconnected(ConnectedCallback callback) { on_client_disconnected_ = callback; }

private:
    int server_fd_{-1};
    std::atomic<int> client_fd_{-1};
    std::atomic<bool> running_{false};
    std::thread thread_;
    MessageCallback on_message_;
    ConnectedCallback on_client_connected_;
    ConnectedCallback on_client_disconnected_;
    void acceptLoop();
};
