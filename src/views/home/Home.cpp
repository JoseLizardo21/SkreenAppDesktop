#include "Home.h"
#include <iostream>
#include "../../services/media/gstreamermanager/GStreamerManager.h"
#include "../../controller/Homecontroller/HomeController.h"


static void on_button_clicked(GtkWidget* widget, gpointer data) {
    Home* self = static_cast<Home*>(data);
    self->requestPermissions(); 
}

void Home::requestPermissions() {
    if (on_request_permissions_callback_) {
        on_request_permissions_callback_();
    }
}


Home::Home() {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Home Window");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Container
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), box);

    // Button
    GtkWidget* button = gtk_button_new_with_label("Transmitir pantalla");
    gtk_widget_set_size_request(button, 200, 50);
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_clicked), this);

    gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
}

void Home::show() {
    gtk_widget_show_all(window);
}