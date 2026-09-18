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

    // message must include the trailing '\n' (message delimiter)
    void send(const std::string& message);

    // Invoked for every JSON line received from the client (offer/answer/ice)
    void setOnMessage(MessageCallback callback) { on_message_ = callback; }
    // Invoked when the client connects, to trigger the WebRTC offer
    void setOnClientConnected(ConnectedCallback callback) { on_client_connected_ = callback; }
    // Invoked when the client disconnects (while the server keeps running), to
    // refresh the pipeline before accepting the next connection
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
