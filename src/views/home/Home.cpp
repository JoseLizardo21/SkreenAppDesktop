#include "Home.h"
#include <iostream>
#include "../../services/media/gstreamermanager/GStreamerManager.h"
#include "../../controller/Homecontroller/HomeController.h"


static void on_button_clicked(GtkWidget* widget, gpointer data) {
    Home* self = static_cast<Home*>(data);
    self->requestPermissions();
}

static void on_cancel_clicked(GtkWidget* widget, gpointer data) {
    Home* self = static_cast<Home*>(data);
    self->cancelTransmission();
}

void Home::requestPermissions() {
    if (on_request_permissions_callback_) {
        on_request_permissions_callback_();
    }
}

void Home::cancelTransmission() {
    if (on_cancel_transmission_callback_) {
        on_cancel_transmission_callback_();
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

    // Transmit button
    transmit_button = gtk_button_new_with_label("Transmitir pantalla");
    gtk_widget_set_size_request(transmit_button, 200, 50);
    gtk_widget_set_sensitive(transmit_button, false);
    g_signal_connect(transmit_button, "clicked", G_CALLBACK(on_button_clicked), this);
    gtk_box_pack_start(GTK_BOX(box), transmit_button, FALSE, FALSE, 0);

    // Cancel button (hidden by default)
    cancel_button = gtk_button_new_with_label("Cancelar transmisión");
    gtk_widget_set_size_request(cancel_button, 200, 50);
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_cancel_clicked), this);
    gtk_box_pack_start(GTK_BOX(box), cancel_button, FALSE, FALSE, 0);
    gtk_widget_set_no_show_all(cancel_button, TRUE);
}

void Home::setTransmitButtonEnabled(bool enabled) {
    gtk_widget_set_sensitive(transmit_button, enabled);
}

void Home::setTransmitting(bool transmitting) {
    if (transmitting) {
        gtk_widget_hide(transmit_button);
        gtk_widget_show(cancel_button);
    } else {
        gtk_widget_show(transmit_button);
        gtk_widget_hide(cancel_button);
    }
}

void Home::show() {
    gtk_widget_show_all(window);
}