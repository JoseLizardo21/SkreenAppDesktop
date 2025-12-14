#ifndef PORTAL_MANAGER_H
#define PORTAL_MANAGER_H

#include <string>
#include <functional>
#include <atomic>
#include <dbus/dbus.h>
#include <cstdint>

/**
 * PortalManager: Manages Wayland portal communication via DBus
 *
 * Handles:
 * - Creating screencast sessions
 * - Selecting sources (monitors)
 * - Starting capture with user dialog
 * - Opening PipeWire remote connections
 *
 * This class encapsulates all DBus portal logic, separating it from UI concerns.
 */
class PortalManager {
public:
    /**
     * Callback signature for portal events:
     * - String: session_handle (object path)
     * - Uint32: pipewire_node_id
     * - Int: pipewire_fd
     */
    using PortalCallback = std::function<void(const std::string&, uint32_t, int)>;
    using ErrorCallback = std::function<void(const std::string&)>;

    PortalManager();
    ~PortalManager();

    /**
     * Set callback for successful portal capture initialization
     */
    void setPortalCallback(PortalCallback callback) { portal_callback_ = callback; }

    /**
     * Set callback for portal errors
     */
    void setErrorCallback(ErrorCallback callback) { error_callback_ = callback; }

    /**
     * Start the asynchronous portal workflow (CreateSession -> SelectSources -> Start)
     * Runs in a separate thread
     */
    void startAsync();

    /**
     * Stop the portal workflow
     */
    void stop();

    /**
     * Check if portal is running
     */
    bool isRunning() const { return is_running_; }

    /**
     * Get current session handle
     */
    const std::string& getSessionHandle() const { return session_handle_; }

    /**
     * Get current PipeWire node ID
     */
    uint32_t getPipeWireNodeId() const { return pipewire_node_id_; }

    /**
     * Get current PipeWire file descriptor
     */
    int getPipeWireFD() const { return pipewire_fd_; }

private:
    // Portal state
    std::string session_handle_;
    uint32_t pipewire_node_id_{0};
    int pipewire_fd_{-1};
    std::atomic<bool> is_running_{false};

    // DBus connection
    DBusConnection* connection_{nullptr};

    // Callbacks
    PortalCallback portal_callback_;
    ErrorCallback error_callback_;

    // Portal workflow
    void createSession();
    void selectSources();
    void startCapture();
    void openPipeWireRemote();

    // DBus message handlers
    void handleCreateSessionResponse(DBusMessage* msg);
    void handleSelectSourcesResponse(DBusMessage* msg);
    void handleStartResponse(DBusMessage* msg);
    void handleOpenPipeWireResponse(DBusMessage* msg);

    // Portal signal handler (static, called from DBus filter)
    static DBusHandlerResult onPortalSignal(DBusConnection* conn, DBusMessage* msg, void* user_data);

    // Helper methods
    void sendDBusMessage(DBusMessage* msg);
    void processSignal(DBusMessage* msg);
    void cleanup();
    void error(const std::string& message);
};

#endif // PORTAL_MANAGER_H
