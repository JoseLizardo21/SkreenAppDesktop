#ifndef WEBSOCKETCLIENT_H
#define WEBSOCKETCLIENT_H

#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <string>
#include <functional>
#include <queue>
#include <condition_variable>

class WebSocketClientImpl;

class WebSocketClient {
public:
    WebSocketClient(const std::string& server_url);
    void connect();
    void disconnect();
    void sendMessage(const std::string& message);

    // Callbacks existentes
    void setOnMessageCallback(std::function<void(const std::string&)> callback) {
        on_message_callback_ = callback;
    }

    void setOnClientInfoCallback(std::function<void(const std::string&, int, int)> callback) {
        on_client_info_callback_ = callback;
    }

    // ========== NUEVO: Callbacks WebRTC ==========
    void setOnSDPCallback(std::function<void(const std::string&, const std::string&)> callback) {
        on_sdp_callback_ = callback;
    }

    void setOnICECandidateCallback(std::function<void(int, const std::string&)> callback) {
        on_ice_callback_ = callback;
    }

    bool isConnected() const {
        return connected_.load();
    }

    ~WebSocketClient();

private:
    static constexpr size_t MAX_QUEUE_SIZE = 100;

    std::string server_url_;
    std::unique_ptr<WebSocketClientImpl> impl_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> running_{false};
    std::thread connect_thread_;
    std::thread message_thread_;
    std::queue<std::string> message_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    // Callbacks
    std::function<void(const std::string&)> on_message_callback_;
    std::function<void(const std::string&, int, int)> on_client_info_callback_;

    // ========== NUEVO: Callbacks WebRTC ==========
    std::function<void(const std::string&, const std::string&)> on_sdp_callback_;  // (type, sdp)
    std::function<void(int, const std::string&)> on_ice_callback_;  // (mline_index, candidate)

    void connectThreadWorker();
    void messageThreadWorker();
    void handleMessage(const std::string& message);
};

#endif
