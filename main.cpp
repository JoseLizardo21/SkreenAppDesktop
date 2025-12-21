#include <iostream>
#include <gtk/gtk.h>
#include "./src/services/media/gstreamermanager/GStreamerManager.h"
#include "./src/views/home/Home.h"
#include "./src/controller/Homecontroller/HomeController.h"
#include "./src/services/websocketclient/WebSocketClient.h"

Home* g_home = nullptr;
HomeController* g_home_controller = nullptr;

WebSocketClient* g_websocket_client = nullptr;

int main(int argc, char* argv[]) {
    GStreamerManager::initializeGStreamer();
    g_websocket_client = new WebSocketClient("ws://localhost:9001");

    gtk_init(&argc, &argv);


    g_home = new Home();
    g_home_controller = new HomeController(g_home);
    g_home->show();
    
    gtk_main();
    return 0;
}