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
        void requestPermissions();
    private:
        GtkWidget* window;
        std::function<void()> on_request_permissions_callback_;

};

#endif