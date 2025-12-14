#include <iostream>
#include <gtk/gtk.h>
#include "./src/services/media/gstreamermanager/GStreamerManager.h"
#include "./src/views/home/Home.h"

Home* g_home = nullptr;

int main(int argc, char* argv[]) {
    GStreamerManager::initializeGStreamer();
    
    gtk_init(&argc, &argv);

    g_home = new Home();
    
    gtk_main();
    return 0;
}