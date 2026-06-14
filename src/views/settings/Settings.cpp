#include "Settings.h"

static const char* SETTINGS_CSS =
    "dialog {"
    "  background-color: #1e1e2e;"
    "}"
    ".settings-section {"
    "  color: #89b4fa;"
    "  font-weight: bold;"
    "  font-size: 11px;"
    "  letter-spacing: 1px;"
    "}"
    ".settings-label {"
    "  color: #cdd6f4;"
    "  font-size: 13px;"
    "}"
    ".settings-hint {"
    "  color: #6c7086;"
    "  font-size: 11px;"
    "}"
    ".settings-unit {"
    "  color: #6c7086;"
    "  font-size: 12px;"
    "  min-width: 52px;"
    "}"
    "spinbutton {"
    "  background-image: none;"
    "  background-color: #313244;"
    "  color: #cdd6f4;"
    "  border: 1px solid #45475a;"
    "  border-radius: 6px;"
    "  min-width: 90px;"
    "}"
    "spinbutton entry {"
    "  background-color: #313244;"
    "  color: #cdd6f4;"
    "  border: none;"
    "  caret-color: #cdd6f4;"
    "}"
    "spinbutton button {"
    "  background-image: none;"
    "  background-color: #313244;"
    "  color: #6c7086;"
    "  border: none;"
    "  border-radius: 0;"
    "}"
    "spinbutton button:hover {"
    "  background-color: #45475a;"
    "  color: #cdd6f4;"
    "}"
    "headerbar button.save-btn {"
    "  background-image: none;"
    "  background-color: #3d84ef;"
    "  color: #ffffff;"
    "  border: none;"
    "  outline: none;"
    "  border-radius: 6px;"
    "  font-weight: bold;"
    "  padding: 4px 14px;"
    "}"
    "headerbar button.save-btn:hover {"
    "  background-color: #5497fc;"
    "}"
    "headerbar button.save-btn:focus {"
    "  outline: none;"
    "  box-shadow: none;"
    "}"
    "headerbar button.cancel-btn {"
    "  background-image: none;"
    "  background-color: #c0394f;"
    "  color: #ffffff;"
    "  border: none;"
    "  outline: none;"
    "  border-radius: 6px;"
    "  font-weight: bold;"
    "  padding: 4px 14px;"
    "}"
    "headerbar button.cancel-btn:hover {"
    "  background-color: #e0425f;"
    "}"
    "headerbar button.cancel-btn:focus {"
    "  outline: none;"
    "  box-shadow: none;"
    "}"
    "button.reset-btn {"
    "  background-image: none;"
    "  background-color: #313244;"
    "  color: #cdd6f4;"
    "  border: 1px solid #585b70;"
    "  border-radius: 6px;"
    "  font-size: 13px;"
    "  font-weight: bold;"
    "  padding: 6px 16px;"
    "}"
    "button.reset-btn:hover {"
    "  background-color: #45475a;"
    "  color: #ffffff;"
    "  border-color: #7f849c;"
    "}"
    "button.reset-btn:focus {"
    "  outline: none;"
    "  box-shadow: none;"
    "}";

static GtkWidget* make_row(GtkWidget* grid, int row,
                            const char* label_text,
                            const char* hint_text,
                            double min, double max, double step, double value,
                            const char* unit) {
    GtkWidget* label = gtk_label_new(label_text);
    gtk_widget_set_halign(label, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(label), "settings-label");

    if (hint_text) {
        GtkWidget* vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
        GtkWidget* hint = gtk_label_new(hint_text);
        gtk_widget_set_halign(hint, GTK_ALIGN_START);
        gtk_style_context_add_class(gtk_widget_get_style_context(hint), "settings-hint");
        gtk_box_pack_start(GTK_BOX(vbox), hint, FALSE, FALSE, 0);
        gtk_widget_set_hexpand(vbox, TRUE);
        gtk_grid_attach(GTK_GRID(grid), vbox, 0, row, 1, 1);
    } else {
        gtk_widget_set_hexpand(label, TRUE);
        gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    }

    GtkWidget* spin = gtk_spin_button_new_with_range(min, max, step);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spin), value);
    gtk_widget_set_halign(spin, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), spin, 1, row, 1, 1);

    GtkWidget* unit_lbl = gtk_label_new(unit);
    gtk_widget_set_halign(unit_lbl, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(unit_lbl), "settings-unit");
    gtk_grid_attach(GTK_GRID(grid), unit_lbl, 2, row, 1, 1);

    return spin;
}

Settings::Settings(GtkWindow* parent, const StreamConfig& current) {
    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, SETTINGS_CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    dialog_ = gtk_dialog_new_with_buttons(
        "Settings",
        parent,
        static_cast<GtkDialogFlags>(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_USE_HEADER_BAR),
        "Cancel", GTK_RESPONSE_CANCEL,
        "Save",   GTK_RESPONSE_ACCEPT,
        NULL);

    gtk_window_set_default_size(GTK_WINDOW(dialog_), 380, -1);
    gtk_window_set_resizable(GTK_WINDOW(dialog_), FALSE);

    GtkWidget* content = gtk_dialog_get_content_area(GTK_DIALOG(dialog_));
    gtk_container_set_border_width(GTK_CONTAINER(content), 20);
    gtk_box_set_spacing(GTK_BOX(content), 16);

    GtkWidget* section = gtk_label_new("STREAM QUALITY");
    gtk_widget_set_halign(section, GTK_ALIGN_START);
    gtk_style_context_add_class(gtk_widget_get_style_context(section), "settings-section");
    gtk_box_pack_start(GTK_BOX(content), section, FALSE, FALSE, 0);

    GtkWidget* grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 16);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 16);
    gtk_box_pack_start(GTK_BOX(content), grid, FALSE, FALSE, 0);

    spin_bitrate_  = make_row(grid, 0, "Video Bitrate",      nullptr,
                               1000, 50000, 500, current.bitrate,          "kbps");
    spin_keyframe_ = make_row(grid, 1, "Keyframe Interval",  nullptr,
                               1,    120,   1,   current.keyframe_interval, "frames");
    spin_speed_    = make_row(grid, 2, "Encoder Speed",      "1 = best quality   7 = fastest",
                               1,    7,     1,   current.encoder_speed,     "");

    GtkWidget* separator = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(content), separator, FALSE, FALSE, 0);

    GtkWidget* reset_btn = gtk_button_new_with_label("Reset to factory defaults");
    gtk_style_context_add_class(gtk_widget_get_style_context(reset_btn), "reset-btn");
    gtk_widget_set_halign(reset_btn, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(content), reset_btn, FALSE, FALSE, 0);

    g_signal_connect(reset_btn, "clicked", G_CALLBACK(+[](GtkButton*, gpointer data) {
        auto* self = static_cast<Settings*>(data);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(self->spin_bitrate_),  4000);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(self->spin_keyframe_), 30);
        gtk_spin_button_set_value(GTK_SPIN_BUTTON(self->spin_speed_),    7);
    }), this);

    GtkWidget* save_btn = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog_), GTK_RESPONSE_ACCEPT);
    if (save_btn) {
        GtkStyleContext* ctx = gtk_widget_get_style_context(save_btn);
        gtk_style_context_remove_class(ctx, "suggested-action");
        gtk_style_context_add_class(ctx, "save-btn");
    }

    GtkWidget* cancel_btn = gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog_), GTK_RESPONSE_CANCEL);
    if (cancel_btn) {
        gtk_style_context_add_class(gtk_widget_get_style_context(cancel_btn), "cancel-btn");
    }

    g_signal_connect(dialog_, "response", G_CALLBACK(on_response), this);

    g_signal_connect(dialog_, "destroy", G_CALLBACK(+[](GtkWidget*, gpointer data) {
        delete static_cast<Settings*>(data);
    }), this);
}

void Settings::show() {
    gtk_widget_show_all(dialog_);
}

void Settings::on_response(GtkDialog* dialog, gint response, gpointer data) {
    auto* self = static_cast<Settings*>(data);
    if (response == GTK_RESPONSE_ACCEPT && self->on_save_) {
        StreamConfig cfg;
        cfg.bitrate           = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(self->spin_bitrate_));
        cfg.keyframe_interval = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(self->spin_keyframe_));
        cfg.encoder_speed     = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(self->spin_speed_));
        self->on_save_(cfg);
    }
    gtk_widget_destroy(GTK_WIDGET(dialog));
}
