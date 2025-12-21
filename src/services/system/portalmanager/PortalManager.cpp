#include "PortalManager.h"
#include <iostream>
#include <thread>
#include <cstring>

PortalManager::PortalManager() = default;

PortalManager::~PortalManager() {
    stop();
    cleanup();
}

void PortalManager::startAsync() {
    if (is_running_) {
        std::cerr << "⚠️ Portal manager already running\n";
        return;
    }

    is_running_ = true;

    // Start portal workflow in a separate thread
    std::thread([this]() {
        DBusError err;
        dbus_error_init(&err);

        connection_ = dbus_bus_get(DBUS_BUS_SESSION, &err);
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
            dbus_connection_read_write_dispatch(connection_, 100);
        }

        dbus_connection_unref(connection_);
        connection_ = nullptr;
    }).detach();
}

void PortalManager::stop() {
    is_running_ = false;
}

void PortalManager::createSession() {
    // std::cout << "Calling CreateSession on portal...\n";

    DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.ScreenCast",
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

    // Add session_handle_token
    DBusMessageIter entry;
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    const char* key = "session_handle_token";
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    DBusMessageIter variant;
    dbus_message_iter_open_container(&entry, DBUS_TYPE_VARIANT, "s", &variant);
    const char* token = "session_12345";
    dbus_message_iter_append_basic(&variant, DBUS_TYPE_STRING, &token);
    dbus_message_iter_close_container(&entry, &variant);
    dbus_message_iter_close_container(&dict, &entry);

    dbus_message_iter_close_container(&args, &dict);

    sendDBusMessage(msg);
    std::cout << "CreateSession sent\n";
}

void PortalManager::selectSources() {
    if (session_handle_.empty()) {
        error("Cannot select sources: no session handle");
        return;
    }

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

    // Options: types = 1 (monitor) and cursor_mode = 2 (embedded)
    DBusMessageIter dict;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);

    // Add types option
    DBusMessageIter entry;
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    const char* key = "types";
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &key);
    DBusMessageIter variant;
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

    std::cout << "Calling Start (portal dialog will appear)...\n";

    DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.ScreenCast",
        "Start"
    );

    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);

    const char* session_path = session_handle_.c_str();
    dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &session_path);

    const char* parent_window = "";
    dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &parent_window);

    // Options with cursor_mode = 2 (embedded)
    DBusMessageIter dict;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);

    DBusMessageIter entry;
    dbus_message_iter_open_container(&dict, DBUS_TYPE_DICT_ENTRY, nullptr, &entry);
    const char* cursor_key = "cursor_mode";
    dbus_message_iter_append_basic(&entry, DBUS_TYPE_STRING, &cursor_key);
    DBusMessageIter variant;
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

    DBusMessage* msg = dbus_message_new_method_call(
        "org.freedesktop.portal.Desktop",
        "/org/freedesktop/portal/desktop",
        "org.freedesktop.portal.ScreenCast",
        "OpenPipeWireRemote"
    );

    if (!msg) {
        error("Failed to create OpenPipeWireRemote message");
        return;
    }

    DBusMessageIter args;
    dbus_message_iter_init_append(msg, &args);

    const char* session_path = session_handle_.c_str();
    dbus_message_iter_append_basic(&args, DBUS_TYPE_OBJECT_PATH, &session_path);

    // Empty options
    DBusMessageIter dict;
    dbus_message_iter_open_container(&args, DBUS_TYPE_ARRAY, "{sv}", &dict);
    dbus_message_iter_close_container(&args, &dict);

    // Send synchronously and get reply
    DBusError err;
    dbus_error_init(&err);

    DBusPendingCall* pending = nullptr;
    if (!dbus_connection_send_with_reply(connection_, msg, &pending, -1)) {
        error("Failed to send OpenPipeWireRemote");
        dbus_message_unref(msg);
        return;
    }

    dbus_connection_flush(connection_);
    dbus_message_unref(msg);

    if (!pending) {
        error("Pending call is NULL");
        return;
    }

    // Block until reply
    dbus_pending_call_block(pending);
    DBusMessage* reply = dbus_pending_call_steal_reply(pending);
    dbus_pending_call_unref(pending);

    if (!reply) {
        error("No reply from OpenPipeWireRemote");
        return;
    }

    // Check for errors
    if (dbus_message_get_type(reply) == DBUS_MESSAGE_TYPE_ERROR) {
        const char* error_name = dbus_message_get_error_name(reply);
        error("OpenPipeWireRemote error: " + std::string(error_name));
        dbus_message_unref(reply);
        return;
    }

    // Extract file descriptor
    int fd = -1;
    dbus_error_init(&err);
    if (dbus_message_get_args(reply, &err,
                              DBUS_TYPE_UNIX_FD, &fd,
                              DBUS_TYPE_INVALID)) {
        pipewire_fd_ = fd;
        // std::cout << "PipeWire FD obtained: " << pipewire_fd_ << "\n";
        // std::cout << "Portal workflow complete!\n";
        // std::cout << "   Session: " << session_handle_ << "\n";
        // std::cout << "   PipeWire Node ID: " << pipewire_node_id_ << "\n";
        // std::cout << "   PipeWire FD: " << pipewire_fd_ << "\n";

        // Call the success callback
        if (portal_callback_) {
            portal_callback_(session_handle_, pipewire_node_id_, pipewire_fd_);
        }
    } else {
        error("Failed to extract file descriptor: " + std::string(err.message));
        dbus_error_free(&err);
    }

    dbus_message_unref(reply);
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

                        if (strcmp(option_key, "size") == 0) {
                            DBusMessageIter size_struct;
                            dbus_message_iter_recurse(&option_variant, &size_struct);
                            int32_t width, height;
                            dbus_message_iter_get_basic(&size_struct, &width);
                            dbus_message_iter_next(&size_struct);
                            dbus_message_iter_get_basic(&size_struct, &height);
                            std::cout << "    Resolution: " << width << "x" << height << "\n";
                        }

                        if (strcmp(option_key, "position") == 0) {
                            DBusMessageIter pos_struct;
                            dbus_message_iter_recurse(&option_variant, &pos_struct);
                            int32_t x, y;
                            dbus_message_iter_get_basic(&pos_struct, &x);
                            dbus_message_iter_next(&pos_struct);
                            dbus_message_iter_get_basic(&pos_struct, &y);
                            std::cout << "    Position: (" << x << ", " << y << ")\n";
                        }

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
    if (!session_handle_found.empty() && session_handle_.empty()) {
        session_handle_ = session_handle_found;
        selectSources();
    } else if (!session_handle_.empty() && !has_streams) {
        startCapture();
    } else if (has_streams) {
        openPipeWireRemote();
    }
}

void PortalManager::sendDBusMessage(DBusMessage* msg) {
    if (!connection_) {
        error("No DBus connection available");
        dbus_message_unref(msg);
        return;
    }

    DBusError err;
    dbus_error_init(&err);

    DBusConnection* conn = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if (!conn) {
        error("Failed to get DBus connection");
        dbus_message_unref(msg);
        return;
    }

    dbus_connection_send(conn, msg, nullptr);
    dbus_connection_flush(conn);
    dbus_connection_unref(conn);
    dbus_message_unref(msg);
}

void PortalManager::cleanup() {
    if (connection_) {
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
