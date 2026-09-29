#ifndef SETTINGS_H
#define SETTINGS_H

#include <gtk/gtk.h>
#include <functional>
#include <string>
#include "config/StreamConfig.h"

class Settings {
public:
    Settings(GtkWindow* parent, const StreamConfig& current);
    void show();
    void setOnSaveCallback(std::function<void(StreamConfig)> cb) { on_save_ = cb; }

    // Called when the user clicks "Add" in the monitor resolution section.
    // Applied immediately (not on Save). Returns true on success and fills
    // 'message' with the text to show under the inputs.
    using AddResolutionHandler = std::function<bool(int width, int height, std::string& message)>;
    void setOnAddResolutionCallback(AddResolutionHandler cb) { on_add_resolution_ = cb; }

private:
    GtkWidget* dialog_;
    GtkWidget* spin_bitrate_;
    GtkWidget* spin_keyframe_;
    GtkWidget* spin_speed_;
    GtkWidget* spin_res_width_;
    GtkWidget* spin_res_height_;
    GtkWidget* res_status_;
    std::function<void(StreamConfig)> on_save_;
    AddResolutionHandler on_add_resolution_;

    static void on_response(GtkDialog* dialog, gint response, gpointer data);
    static void on_add_resolution(GtkButton* button, gpointer data);
};

#endif
