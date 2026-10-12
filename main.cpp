#include <iostream>
#include <gtk/gtk.h>
#include <glib-unix.h>
#include "./src/services/media/gstreamermanager/GStreamerManager.h"
#include "./src/views/home/Home.h"
#include "./src/controller/Homecontroller/HomeController.h"

Home* g_home = nullptr;
HomeController* g_home_controller = nullptr;

int main(int argc, char* argv[]) {
    GStreamerManager::initializeGStreamer();

    gtk_init(&argc, &argv);

    g_home = new Home();
    g_home_controller = new HomeController(g_home);
    g_home->show();

    // Leave the main loop on SIGINT/SIGTERM (Ctrl+C, logout) so the cleanup
    // below still runs.
    auto quit = +[](gpointer) -> gboolean { gtk_main_quit(); return G_SOURCE_CONTINUE; };
    g_unix_signal_add(SIGINT, quit, nullptr);
    g_unix_signal_add(SIGTERM, quit, nullptr);

    gtk_main();

    g_home_controller->handleAppExit();
    return 0;
}
