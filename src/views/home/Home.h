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
        // Llamado cuando el usuario mueve el switch. Debe devolver true si el
        // monitor quedó en el estado pedido (enabled), false si falló y el
        // switch debe revertir visualmente.
        void setOnMonitorToggleCallback(std::function<bool(bool)> callback) {
            on_monitor_toggle_callback_ = callback;
        }
        void requestPermissions();
        void cancelTransmission();
        void openSettings();
        bool monitorToggled(bool enabled);
        // Refleja el estado real del monitor en el switch sin disparar
        // setOnMonitorToggleCallback (usar al inicializar la UI).
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
