#include "Home.h"
#include <iostream>
#include "../../services/media/gstreamermanager/GStreamerManager.h"

static void on_button_clicked(GtkWidget* widget, gpointer data) {
    GStreamerManager* capture_manager_{nullptr};
    // 2. AHORA crear el pipeline (rápido, ~50ms)
    capture_manager_ = new GStreamerManager();

    // capture_manager_->initializePipeline(fd, node_id, 1280, 720);

    // 3. Iniciar captura inmediatamente
    capture_manager_->startCapture();

    std::cout << "✅ Captura iniciada\n";
    std::cout << "Hola" << std::endl;
}

Home::Home() {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Home Window");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Contenedor
    GtkWidget* box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_add(GTK_CONTAINER(window), box);

    // Botón
    GtkWidget* button = gtk_button_new_with_label("Saludar");
    gtk_widget_set_size_request(button, 200, 50);
    g_signal_connect(button, "clicked", G_CALLBACK(on_button_clicked), NULL);

    gtk_box_pack_start(GTK_BOX(box), button, FALSE, FALSE, 0);
}

void Home::show() {
    gtk_widget_show_all(window);
}