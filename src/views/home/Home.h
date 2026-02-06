#ifndef HOME_H
#define HOME_H

#include <memory>
#include <gtk/gtk.h>
#include <functional>

// Forward declaration
class HomeController;

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
        void requestPermissions();
        void cancelTransmission();
        void setTransmitButtonEnabled(bool enabled);
        void setTransmitting(bool transmitting);
    private:
        GtkWidget* window;
        GtkWidget* transmit_button;
        GtkWidget* cancel_button;
        std::function<void()> on_request_permissions_callback_;
        std::function<void()> on_cancel_transmission_callback_;

};

#endif