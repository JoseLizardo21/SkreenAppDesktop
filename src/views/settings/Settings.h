#ifndef SETTINGS_H
#define SETTINGS_H

#include <gtk/gtk.h>
#include <functional>
#include "config/StreamConfig.h"

class Settings {
public:
    Settings(GtkWindow* parent, const StreamConfig& current);
    void show();
    void setOnSaveCallback(std::function<void(StreamConfig)> cb) { on_save_ = cb; }

private:
    GtkWidget* dialog_;
    GtkWidget* spin_bitrate_;
    GtkWidget* spin_keyframe_;
    GtkWidget* spin_speed_;
    std::function<void(StreamConfig)> on_save_;

    static void on_response(GtkDialog* dialog, gint response, gpointer data);
};

#endif
