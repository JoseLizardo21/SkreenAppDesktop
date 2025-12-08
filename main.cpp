#include <iostream>
#include <gtk/gtk.h>
#include "./src/views/home/Home.h"

Home* g_home = nullptr;

int main(int argc, char* argv[]) {
    gtk_init(&argc, &argv);

    g_home = new Home();
    
    gtk_main();
    return 0;
}