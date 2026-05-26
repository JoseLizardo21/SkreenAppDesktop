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
        void requestPermissions();
        void cancelTransmission();
        void openSettings();
        void setTransmitButtonEnabled(bool enabled);
        void setTransmitting(bool transmitting);
        void setDeviceConnected(bool connected);
        GtkWindow* getGtkWindow() { return GTK_WINDOW(window); }
    private:
        GtkWidget* window;
        GtkWidget* transmit_button;
        GtkWidget* cancel_button;
        GtkWidget* status_label;
        GtkWidget* status_dot;
        std::function<void()> on_request_permissions_callback_;
        std::function<void()> on_cancel_transmission_callback_;
        std::function<void()> on_settings_callback_;
};

#endif
