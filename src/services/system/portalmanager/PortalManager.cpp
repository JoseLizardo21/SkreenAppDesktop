#include "PortalManager.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <cstring>
#include <mutex>

PortalManager::PortalManager() = default;

PortalManager::~PortalManager() {
    stop();
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
    cleanup();
}

void PortalManager::startAsync() {
    // Stop previous session if running
    if (is_running_) {
        stop();
    }

    // Wait for previous thread to finish
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    // Cleanup previous connection
    cleanup();

    // Reset state
    session_handle_.clear();
    session_token_.clear();
    request_token_.clear();
    pipewire_node_id_ = 0;
    pipewire_fd_ = -1;
    state_ = State::IDLE;

    is_running_ = true;

    // Start portal workflow in a separate thread
    worker_thread_ = std::thread([this]() {
        DBusError err;
        dbus_error_init(&err);

        // Conexión privada (no compartida con el resto del proceso): al
        // cerrarla en stop() el bus notifica la desconexión y Mutter limpia
        // por completo la sesión de ScreenCast/RemoteDesktop asociada (mismo
        // mecanismo de "cliente desaparecido" que libera el stream de
        // PipeWire). Con la conexión compartida de dbus_bus_get(), el
        // proceso nunca "desaparece" entre sesiones y el node_id reciclado
        // de la siguiente captura se queda sin frames.
        connection_ = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
        if (dbus_error_is_set(&err)) {
            std::cerr << "⚠️  DBus connection failed: " << err.message << "\n";
            std::cerr << "   Attempting to set up DBus session...\n";
            dbus_error_free(&err);

            // Try to initialize DBus from environment or current session
            dbus_error_init(&err);

            // Check if we can get address from environment
            const char* addr = getenv("DBUS_SESSION_BUS_ADDRESS");
            if (addr) {
                std::cerr << "   Found DBUS_SESSION_BUS_ADDRESS: " << addr << "\n";
                connection_ = dbus_bus_get_private(DBUS_BUS_SESSION, &err);
            } else {
                std::cerr << "   DBUS_SESSION_BUS_ADDRESS not set\n";
                std::cerr << "   Make sure systemd-user-session is active\n";
            }

            if (dbus_error_is_set(&err) || !connection_) {
                error("Failed to connect to DBus session bus: " + std::string(err.message ? err.message : "Unknown error"));
                dbus_error_free(&err);
                is_running_ = false;
                return;
            }
        }

        std::cout << "Connected to DBus session bus (Wayland portal)\n";

        // Add listener for portal Response signals
        dbus_bus_add_match(connection_,
            "type='signal',interface='org.freedesktop.portal.Request',member='Response'",
            &err);
        dbus_connection_add_filter(connection_, onPortalSignal, this, nullptr);

        // Start the portal workflow
        createSession();

        // Keep connection alive listening for signals
        while (is_running_) {
            dbus_connection_read_write_dispatch(connection_, 5);
            processFdRequests();
        }

        // dbus_connection_close() es lo que hace que el bus notifique
        // nuestra desconexión a Mutter (ver comentario más arriba)
        dbus_connection_close(connection_);
        dbus_connection_unref(connection_);
        connection_ = nullptr;
    });
}

void PortalManager::stop() {
    // El bus session de DBus es compartido por todo el proceso (dbus_bus_get
    // devuelve la misma conexión en cada startAsync()), así que la sesión del
    // portal de la captura anterior nunca se cierra sola al "desconectar". Si
    // no se cierra explícitamente, xdg-desktop-portal/el compositor la dejan
    // activa indefinidamente y la siguiente sesión (nuevo node_id) puede no
    // recibir frames de PipeWire.
    closeSession();
    is_running_ = false;
}

void PortalManager::closeSession() {
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (!connection_ || session_handle_.empty())
        return;

    DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        session_handle_.c_str(),
        "org.freedesktop.portal.Session",
        "Close"
    );
    if (!msg)
        return;

    dbus_connection_send(connection_, msg, nullptr);
    dbus_connection_flush(connection_);
    dbus_message_unref(msg);
    std::cout << "Portal session closed: " << session_handle_ << "\n";
    session_handle_.clear();
}

void PortalManager::createSession() {
    // std::cout << "Calling CreateSession on portal...\n";
    DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.RemoteDesktop",
        "CreateSession"
    );

    if (!msg) {
        error("Failed to create DBus message for CreateSession");
        return;
    }

    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);

    // Add options (empty dict for now)
    DBusMessageIter dict;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);

    // Generate unique tokens
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    session_token_ = "session" + std::to_string(now);
    request_token_ = "request" + std::to_string(++request_counter_) + "_" + std::to_string(now);

    // Add handle_token (for filtering responses)
    DBusMessageIter entry;
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    const char* handle_key = "handle_token";
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &handle_key);
    DBusMessageIter variant;
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
    const char* req_token = request_token_.c_str();
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &req_token);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(&dict, &entry);

    // Add session_handle_token
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    const char* key = "session_handle_token";
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
    const char* token = session_token_.c_str();
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &token);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(&dict, &entry);

    dbus_message_iter_close_container(&args, &dict);

    sendDBusMessage(msg);
    std::cout << "CreateSession sent\n";
}

void PortalManager::selectDevices() {
    if (session_handle_.empty()) {
        error("Cannot select devices: no session handle");
        return;
    }

    // Update request token
    request_token_ = "request" + std::to_string(++request_counter_) + "_" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

    DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.RemoteDesktop",
        "SelectDevices"
    );

    if (!msg) {
        error("Failed to create DBus message for SelectDevices");
        return;
    }

    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);

    const char* session_path = session_handle_.c_str();
    dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &session_path);

    // Options
    DBusMessageIter dict;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);

    // Add handle_token
    DBusMessageIter entry;
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    const char* handle_key = "handle_token";
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &handle_key);
    DBusMessageIter variant;
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
    const char* req_token = request_token_.c_str();
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &req_token);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(&dict, &entry);

    // Add types option: 6 = pointer (2) + touch (4)
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    const char* types_key = "types";
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &types_key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "u", &variant);
    uint32_t types = 6;
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_UINT32, &types);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(&dict, &entry);

    dbus_message_iter_close_container(&args, &dict);

    sendDBusMessage(msg);
    std::cout << "SelectDevices sent\n";
}

void PortalManager::selectSources() {
    if (session_handle_.empty()) {
        error("Cannot select sources: no session handle");
        return;
    }

    // Update request token
    request_token_ = "request" + std::to_string(++request_counter_) + "_" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

    DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.ScreenCast",
        "SelectSources"
    );

    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);

    const char* session_path = session_handle_.c_str();
    dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &session_path);

    // Options
    DBusMessageIter dict;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);

    // Add handle_token
    DBusMessageIter entry;
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    const char* handle_key = "handle_token";
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &handle_key);
    DBusMessageIter variant;
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
    const char* req_token = request_token_.c_str();
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &req_token);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(&dict, &entry);

    // Add types option
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    const char* key = "types";
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "u", &variant);
    uint32_t types = 1;  // Monitor
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_UINT32, &types);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(&dict, &entry);

    // Add cursor_mode option
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    const char* cursor_key = "cursor_mode";
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &cursor_key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "u", &variant);
    uint32_t cursor_mode = 2;  // Embedded cursor
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_UINT32, &cursor_mode);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(&dict, &entry);

    dbus_message_iter_close_container(&args, &dict);

    sendDBusMessage(msg);
    std::cout << "SelectSources sent\n";
}

void PortalManager::startCapture() {
    if (session_handle_.empty()) {
        error("Cannot start capture: no session handle");
        return;
    }

    // Update request token
    request_token_ = "request" + std::to_string(++request_counter_) + "_" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

    std::cout << "Calling Start (portal dialog will appear)...\n";

    DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.RemoteDesktop",
        "Start"
    );

    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);

    const char* session_path = session_handle_.c_str();
    dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &session_path);

    const char* parent_window = "";
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &parent_window);

    // Options
    DBusMessageIter dict;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);

    // Add handle_token
    DBusMessageIter entry;
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    const char* handle_key = "handle_token";
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &handle_key);
    DBusMessageIter variant;
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
    const char* req_token = request_token_.c_str();
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &req_token);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(&dict, &entry);

    // Add cursor_mode
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    const char* cursor_key = "cursor_mode";
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &cursor_key);
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "u", &variant);
    uint32_t cursor_mode = 2;  // Embedded cursor
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_UINT32, &cursor_mode);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(&dict, &entry);

    dbus_message_iter_close_container(&args, &dict);

    sendDBusMessage(msg);
    std::cout << "Start sent - waiting for user selection...\n";
}

void PortalManager::openPipeWireRemote() {
    if (session_handle_.empty()) {
        error("Cannot open PipeWire remote: no session handle");
        return;
    }

    if (!connection_) {
        error("No DBus connection available");
        return;
    }

    std::cout << "Requesting PipeWire file descriptor...\n";

    int fd = requestPipeWireFdSync();
    if (fd < 0) {
        error("Failed to obtain PipeWire file descriptor");
        return;
    }

    pipewire_fd_ = fd;

    if (portal_callback_) {
        portal_callback_(session_handle_, pipewire_node_id_, pipewire_fd_);
    }
}

int PortalManager::requestPipeWireFdSync() {
    if (session_handle_.empty() || !connection_)
        return -1;

    DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.ScreenCast",
        "OpenPipeWireRemote"
    );

    if (!msg)
        return -1;

    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);

    const char* session_path = session_handle_.c_str();
    dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &session_path);

    // Empty options
    DBusMessageIter dict;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);
    dbus_message_iter_close_container(&args, &dict);

    DBusPendingCall* pending = nullptr;
    if (!dbus_connection_send_with_reply(connection_, msg, &pending, -1)) {
        dbus_message_unref(msg);
        return -1;
    }

    dbus_connection_flush(connection_);
    dbus_message_unref(msg);

    if (!pending)
        return -1;

    // Block until reply (corre en worker_thread_, no comparte connection_ con otro hilo)
    dbus_pending_call_block(pending);
    DBusMessage* reply = dbus_pending_call_steal_reply(pending);
    dbus_pending_call_unref(pending);

    if (!reply)
        return -1;

    if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
        const char* error_name = dbus_message_get_error_name(reply);
        std::cerr << "Portal Error: OpenPipeWireRemote error: " << error_name << "\n";
        dbus_message_unref(reply);
        return -1;
    }

    int fd = -1;
    DBusError err;
    dbus_error_init(&err);
    if (!dbus_message_get_args(reply, &err,
                               DBUS_TYPE_UNIX_FD, &fd,
                               DBUS_TYPE_INVALID)) {
        std::cerr << "Portal Error: Failed to extract file descriptor: " << err.message << "\n";
        dbus_error_free(&err);
        fd = -1;
    }

    dbus_message_unref(reply);
    return fd;
}

void PortalManager::requestPipeWireFd(FdCallback callback) {
    std::lock_guard<std::mutex> lock(fd_requests_mutex_);
    fd_requests_.push_back(std::move(callback));
}

void PortalManager::processFdRequests() {
    std::vector<FdCallback> pending;
    {
        std::lock_guard<std::mutex> lock(fd_requests_mutex_);
        if (fd_requests_.empty())
            return;
        pending.swap(fd_requests_);
    }

    for (auto& cb : pending) {
        int fd = requestPipeWireFdSync();
        cb(fd);
    }
}

DBusHandlerResult PortalManager::onPortalSignal(DBusConnection* conn, DBusMessage* msg, void* user_data) {
    (void)conn;  // Unused
    PortalManager* self = static_cast<PortalManager*>(user_data);

    if (!dbus_message_is_signal(msg, "org.freedesktop.portal.Request", "Response"))
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

    self->processSignal(msg);
    return DBUS_HANDLER_RESULT_HANDLED;
}

void PortalManager::processSignal(DBusMessage* msg) {
    const char* path = dbus_message_get_path(msg);

    // Ignore signals from previous requests
    std::string path_str(path ? path : "");
    if (!request_token_.empty() && path_str.find(request_token_) == std::string::npos) {
        // This signal is not for our current request, ignore it
        return;
    }

    DBusMessageIter args;
    dbus_message_iter_init(msg, &args);

    uint32_t response = 0;
    dbus_message_iter_get_basic(&args, &response);

    // 0=success, 1=cancelled, 2=error

    if (response != 0) {
        error("User cancelled or error occurred");
        is_running_ = false;
        return;
    }

    DBusMessageIter dict;
    dbus_message_iter_next(&args);
    dbus_message_iter_recurse(&args, &dict);

    std::string session_handle_found;
    bool has_streams = false;

    // Parse response dictionary
    while (dbus_message_iter_get_arg_type(&dict) != DBUS_TYPE_INVALID) {
        DBusMessageIter entry;
        dbus_message_iter_recurse(&dict, &entry);

        const char* key;
        dbus_message_iter_get_basic(&entry, &key);
        dbus_message_iter_next(&entry);

        DBusMessageIter variant;
        dbus_message_iter_recurse(&entry, &variant);

        // Handle session_handle (after CreateSession)
        if (strcmp(key, "session_handle") == 0) {
            const char* handle;
            dbus_message_iter_get_basic(&variant, &handle);
            session_handle_found = handle;
            std::cout << "Session handle: " << handle << "\n";
        }

        // Handle streams (after Start)
        if (strcmp(key, "streams") == 0) {
            has_streams = true;
            std::cout << "Processing streams...\n";

            DBusMessageIter streams_array;
            dbus_message_iter_recurse(&variant, &streams_array);

            int stream_count = 0;
            while (dbus_message_iter_get_arg_type(&streams_array) == DBUS_TYPE_STRUCT) {
                DBusMessageIter stream_struct;
                dbus_message_iter_recurse(&streams_array, &stream_struct);

                // Extract node_id (uint32)
                uint32_t node_id;
                dbus_message_iter_get_basic(&stream_struct, &node_id);

                std::cout << "  Stream " << stream_count << " - Node ID: " << node_id << "\n";

                if (stream_count == 0) {
                    pipewire_node_id_ = node_id;
                }

                // Parse stream options
                dbus_message_iter_next(&stream_struct);
                if (dbus_message_iter_get_arg_type(&stream_struct) == DBUS_TYPE_ARRAY) {
                    DBusMessageIter stream_options;
                    dbus_message_iter_recurse(&stream_struct, &stream_options);

                    while (dbus_message_iter_get_arg_type(&stream_options) != DBUS_TYPE_INVALID) {
                        DBusMessageIter option_entry;
                        dbus_message_iter_recurse(&stream_options, &option_entry);

                        const char* option_key;
                        dbus_message_iter_get_basic(&option_entry, &option_key);
                        dbus_message_iter_next(&option_entry);

                        DBusMessageIter option_variant;
                        dbus_message_iter_recurse(&option_entry, &option_variant);

                        dbus_message_iter_next(&stream_options);
                    }
                }

                stream_count++;
                dbus_message_iter_next(&streams_array);
            }

            std::cout << "Total streams: " << stream_count << "\n";
        }

        dbus_message_iter_next(&dict);
    }

    // State machine for portal workflow
    if (!session_handle_found.empty() && state_ == State::IDLE) {
        session_handle_ = session_handle_found;
        state_ = State::SESSION_CREATED;
        selectDevices();
    } else if (session_handle_found.empty() && !has_streams && state_ == State::SESSION_CREATED) {
        state_ = State::DEVICES_SELECTED;
        selectSources();
    } else if (session_handle_found.empty() && !has_streams && state_ == State::DEVICES_SELECTED) {
        state_ = State::SOURCES_SELECTED;
        startCapture();
    } else if (has_streams) {
        openPipeWireRemote();
    }
}

void PortalManager::notifyPointerMotionAbsolute(double x, double y) {
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (!connection_ || session_handle_.empty()) return;
    DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.RemoteDesktop",
        "NotifyPointerMotionAbsolute"
    );
    if (!msg) return;
    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);
    const char* sp = session_handle_.c_str();
    dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &sp);
    DBusMessageIter dict;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);
    dbus_message_iter_close_container(&args, &dict);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &pipewire_node_id_);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_DOUBLE, &x);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_DOUBLE, &y);
    dbus_connection_send(connection_, msg, nullptr);
    dbus_message_unref(msg);
}

void PortalManager::notifyPointerButton(int32_t button, uint32_t state) {
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (!connection_ || session_handle_.empty()) return;
    DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.RemoteDesktop",
        "NotifyPointerButton"
    );
    if (!msg) return;
    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);
    const char* sp = session_handle_.c_str();
    dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &sp);
    DBusMessageIter dict;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);
    dbus_message_iter_close_container(&args, &dict);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &button);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_UINT32, &state);
    dbus_connection_send(connection_, msg, nullptr);
    dbus_message_unref(msg);
}

void PortalManager::notifyPointerAxis(double dx, double dy) {
    std::lock_guard<std::mutex> lock(send_mutex_);
    if (!connection_ || session_handle_.empty()) return;
    DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.RemoteDesktop",
        "NotifyPointerAxis"
    );
    if (!msg) return;
    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);
    const char* sp = session_handle_.c_str();
    dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &sp);
    DBusMessageIter dict;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);
    dbus_message_iter_close_container(&args, &dict);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_DOUBLE, &dx);
    dbus_message_iter_append_basic(&args, DBUS_TYPE_DOUBLE, &dy);
    dbus_connection_send(connection_, msg, nullptr);
    dbus_message_unref(msg);
}

void PortalManager::sendDBusMessage(DBusMessage* msg) {
    if (!connection_) {
        error("No DBus connection available");
        dbus_message_unref(msg);
        return;
    }

    dbus_connection_send(connection_, msg, nullptr);
    dbus_connection_flush(connection_);
    dbus_message_unref(msg);
}

void PortalManager::cleanup() {
    if (connection_) {
        dbus_connection_close(connection_);
        dbus_connection_unref(connection_);
        connection_ = nullptr;
    }
}

void PortalManager::error(const std::string& message) {
    std::cerr << "Portal Error: " << message << "\n";
    is_running_ = false;
    if (error_callback_) {
        error_callback_(message);
    }
}
