#ifndef HOME_H
#define HOME_H

#include <memory>
#include <gtk/gtk.h>
#include <functional>
#include <string>
#include <vector>
#include "../../config/ConnectionMode.h"

class Home {
    public:
        Home();
        void show();
        void setOnRequestPermissionsCallback(std::function<void()> callback) {
            on_request_permissions_callback_ = callback;
        }
        void setOnCancelTransmissionCallback(std::function<void()> callback) {
            on_cancel_transmission_callback_ = callback;
        }
        void setOnSettingsCallback(std::function<void()> callback) {
            on_settings_callback_ = callback;
        }
        void setOnConnectionModeChangedCallback(std::function<void(ConnectionMode)> callback) {
            on_connection_mode_changed_callback_ = callback;
        }
        // Called when the user moves the switch. Should return true if the
        // monitor ended up in the requested state (enabled), false if it
        // failed and the switch should revert visually.
        void setOnMonitorToggleCallback(std::function<bool(bool)> callback) {
            on_monitor_toggle_callback_ = callback;
        }
        void requestPermissions();
        void cancelTransmission();
        void openSettings();
        bool monitorToggled(bool enabled);
        // Invocado por el handler GTK del radio de WiFi cuando el usuario cambia el modo.
        void connectionModeToggled(bool wifi_selected);
        // Shows the confirmation dialog for turning off the monitor. Returns
        // true if the user confirmed.
        bool confirmDisableMonitor();
        // Locks the switch in its current position (insensitive) because
        // turning the monitor back on in this GNOME session would break
        // mutter.
        void lockMonitorSwitch();
        // Reflects the real monitor state on the switch without triggering
        // setOnMonitorToggleCallback (used when initializing the UI).
        void setMonitorSwitchState(bool enabled);
        void setTransmitButtonEnabled(bool enabled);
        void setTransmitting(bool transmitting);
        void setDeviceConnected(bool connected);
        // Estado del status_row en modo WiFi (no depende de detección adb).
        void setWifiReady(bool ready);
        // Refleja el modo guardado en la UI sin disparar setOnConnectionModeChangedCallback
        // (mismo patrón que setMonitorSwitchState).
        void setConnectionMode(ConnectionMode mode);
        // Label con la(s) IP(s) local(es); pasar vector vacío la oculta (modo Cable).
        void setLocalIpAddresses(const std::vector<std::string>& ips);
        // Deshabilita el selector Cable/WiFi mientras hay una sesión de streaming activa.
        void setConnectionModeSelectorEnabled(bool enabled);
        GtkWindow* getGtkWindow() { return GTK_WINDOW(window); }
    private:
        // Feature flag for the monitor enable/disable switch, read from the
        // SKREEN_ACTIVE_MODULE_DRIVER env var (set to "1" to enable). Off by
        // default until the feature is ready to ship.
        static bool activeModuleDriver();

        GtkWidget* window;
        GtkWidget* monitor_switch = nullptr;
        gulong monitor_switch_handler_id_ = 0;
        GtkWidget* transmit_button;
        GtkWidget* cancel_button;
        GtkWidget* status_label;
        GtkWidget* status_dot;
        GtkWidget* cable_radio_ = nullptr;
        GtkWidget* wifi_radio_ = nullptr;
        gulong connection_mode_handler_id_ = 0;
        GtkWidget* local_ip_label_ = nullptr;
        std::function<void()> on_request_permissions_callback_;
        std::function<void()> on_cancel_transmission_callback_;
        std::function<void()> on_settings_callback_;
        std::function<bool(bool)> on_monitor_toggle_callback_;
        std::function<void(ConnectionMode)> on_connection_mode_changed_callback_;
};

#endif
