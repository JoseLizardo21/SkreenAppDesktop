#include "Home.h"
#include <iostream>

Home::Home() {
    gtk_init(nullptr, nullptr);
    displayHome();
}

void Home::displayHome()  {
    GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Home Window");
    gtk_window_set_default_size(GTK_WINDOW(window), 600, 400);

    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(window);
    gtk_main();
}