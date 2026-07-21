#ifndef HOME_H
#define HOME_H

#include <memory>
#include <gtk/gtk.h>
#include <functional>

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
        GtkWindow* getGtkWindow() { return GTK_WINDOW(window); }
    private:
        GtkWidget* window;
        GtkWidget* monitor_switch;
        gulong monitor_switch_handler_id_ = 0;
        GtkWidget* transmit_button;
        GtkWidget* cancel_button;
        GtkWidget* status_label;
        GtkWidget* status_dot;
        std::function<void()> on_request_permissions_callback_;
        std::function<void()> on_cancel_transmission_callback_;
        std::function<void()> on_settings_callback_;
        std::function<bool(bool)> on_monitor_toggle_callback_;
};

#endif
