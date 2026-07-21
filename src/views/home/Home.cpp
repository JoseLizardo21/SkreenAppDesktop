#include "Home.h"
#include "../../controller/Homecontroller/HomeController.h"
#include "version.h"

static void on_button_clicked(GtkWidget*, gpointer data) {
    static_cast<Home*>(data)->requestPermissions();
}

static void on_cancel_clicked(GtkWidget*, gpointer data) {
    static_cast<Home*>(data)->cancelTransmission();
}

static void on_settings_clicked(GtkWidget*, gpointer data) {
    static_cast<Home*>(data)->openSettings();
}

void Home::requestPermissions() {
    if (on_request_permissions_callback_) on_request_permissions_callback_();
}

void Home::cancelTransmission() {
    if (on_cancel_transmission_callback_) on_cancel_transmission_callback_();
}

void Home::openSettings() {
    if (on_settings_callback_) on_settings_callback_();
}

static const char* APP_CSS =
    "window {"
    "  background-color: #1e1e2e;"
    "}"
    "headerbar, headerbar.titlebar {"
    "  background-image: none;"
    "  background-color: #181825;"
    "  border-bottom: 1px solid #313244;"
    "  padding: 4px 8px;"
    "  box-shadow: none;"
    "}"
    "headerbar label {"
    "  color: #cdd6f4;"
    "}"
    "headerbar .title {"
    "  font-weight: bold;"
    "  font-size: 14px;"
    "}"
    "headerbar .subtitle {"
    "  color: #6c7086;"
    "  font-size: 11px;"
    "}"
    "headerbar button.titlebutton {"
    "  background-image: none;"
    "  background-color: transparent;"
    "  color: #6c7086;"
    "  border: none;"
    "  border-radius: 6px;"
    "  padding: 4px;"
    "  min-width: 16px;"
    "  min-height: 16px;"
    "}"
    "headerbar button.titlebutton:hover {"
    "  background-color: rgba(205,214,244,0.12);"
    "  color: #cdd6f4;"
    "}"
    "headerbar button.titlebutton.close:hover {"
    "  background-color: #f38ba8;"
    "  color: #1e1e2e;"
    "}"
    "headerbar button.titlebutton image {"
    "  color: inherit;"
    "}"
    ".main-icon {"
    "  color: #89b4fa;"
    "  opacity: 0.9;"
    "}"
    ".monitor-icon-off {"
    "  color: #6c7086;"
    "}"
    ".monitor-icon-on {"
    "  color: #89b4fa;"
    "}"
    ".status-row label {"
    "  color: #6c7086;"
    "  font-size: 13px;"
    "}"
    ".dot-idle {"
    "  color: #6c7086;"
    "  font-size: 16px;"
    "}"
    ".dot-ready {"
    "  color: #89b4fa;"
    "  font-size: 16px;"
    "}"
    ".dot-active {"
    "  color: #a6e3a1;"
    "  font-size: 16px;"
    "}"
    ".status-idle {"
    "  color: #6c7086;"
    "  font-size: 13px;"
    "}"
    ".status-ready {"
    "  color: #89b4fa;"
    "  font-size: 13px;"
    "}"
    ".status-active {"
    "  color: #a6e3a1;"
    "  font-size: 13px;"
    "}"
    "button.transmit-btn {"
    "  background-image: none;"
    "  background-color: #89b4fa;"
    "  color: #1e1e2e;"
    "  border-radius: 12px;"
    "  border: none;"
    "  padding: 10px 32px;"
    "  font-weight: bold;"
    "  font-size: 14px;"
    "  box-shadow: 0 2px 10px rgba(137,180,250,0.35);"
    "}"
    "button.transmit-btn:hover {"
    "  background-color: #b4d0ff;"
    "  box-shadow: 0 4px 14px rgba(137,180,250,0.5);"
    "}"
    "button.transmit-btn:active {"
    "  background-color: #74a8e8;"
    "}"
    "button.transmit-btn:disabled {"
    "  background-color: #313244;"
    "  color: #6c7086;"
    "  box-shadow: none;"
    "}"
    "button.cancel-btn {"
    "  background-image: none;"
    "  background-color: #f38ba8;"
    "  color: #1e1e2e;"
    "  border-radius: 12px;"
    "  border: none;"
    "  padding: 10px 32px;"
    "  font-weight: bold;"
    "  font-size: 14px;"
    "  box-shadow: 0 2px 10px rgba(243,139,168,0.35);"
    "}"
    "button.cancel-btn:hover {"
    "  background-color: #ffb3c6;"
    "  box-shadow: 0 4px 14px rgba(243,139,168,0.5);"
    "}"
    "button.cancel-btn:active {"
    "  background-color: #e07090;"
    "}"
    ".version-label {"
    "  color: #7f849c;"
    "  font-size: 11px;"
    "}";

Home::Home() {
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_default_size(GTK_WINDOW(window), 420, 300);
    gtk_window_set_resizable(GTK_WINDOW(window), FALSE);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget* header = gtk_header_bar_new();
    gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header), TRUE);
    gtk_header_bar_set_title(GTK_HEADER_BAR(header), "Skreen");
    gtk_header_bar_set_subtitle(GTK_HEADER_BAR(header), "Desktop Streamer");
    gtk_window_set_titlebar(GTK_WINDOW(window), header);

    GtkWidget* settings_btn = gtk_button_new();
    GtkWidget* settings_icon = gtk_image_new_from_icon_name("preferences-system-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(settings_btn), settings_icon);
    gtk_style_context_add_class(gtk_widget_get_style_context(settings_btn), "titlebutton");
    gtk_widget_set_tooltip_text(settings_btn, "Settings");
    g_signal_connect(settings_btn, "clicked", G_CALLBACK(on_settings_clicked), this);
    gtk_header_bar_pack_end(GTK_HEADER_BAR(header), settings_btn);

    GtkCssProvider* css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css, APP_CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(css),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(css);

    GtkWidget* outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), outer);

    GtkWidget* monitor_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(monitor_box, GTK_ALIGN_END);
    gtk_widget_set_margin_top(monitor_box, 12);
    gtk_widget_set_margin_end(monitor_box, 16);

    GtkWidget* monitor_off_icon = gtk_image_new_from_icon_name("video-display-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_style_context_add_class(gtk_widget_get_style_context(monitor_off_icon), "monitor-icon-off");
    gtk_box_pack_start(GTK_BOX(monitor_box), monitor_off_icon, FALSE, FALSE, 0);

    monitor_switch = gtk_switch_new();
    gtk_widget_set_valign(monitor_switch, GTK_ALIGN_CENTER);
    gtk_widget_set_tooltip_text(monitor_switch, "Enable monitor");
    gtk_box_pack_start(GTK_BOX(monitor_box), monitor_switch, FALSE, FALSE, 0);

    GtkWidget* monitor_on_icon = gtk_image_new_from_icon_name("video-display-symbolic", GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_style_context_add_class(gtk_widget_get_style_context(monitor_on_icon), "monitor-icon-on");
    gtk_box_pack_start(GTK_BOX(monitor_box), monitor_on_icon, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(outer), monitor_box, FALSE, FALSE, 0);

    GtkWidget* center = gtk_box_new(GTK_ORIENTATION_VERTICAL, 18);
    gtk_widget_set_valign(center, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(center, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(center, 24);
    gtk_widget_set_margin_bottom(center, 32);
    gtk_box_pack_start(GTK_BOX(outer), center, TRUE, TRUE, 0);

    GtkWidget* icon = gtk_image_new_from_icon_name("video-display-symbolic", GTK_ICON_SIZE_DIALOG);
    gtk_image_set_pixel_size(GTK_IMAGE(icon), 80);
    gtk_style_context_add_class(gtk_widget_get_style_context(icon), "main-icon");
    gtk_widget_set_halign(icon, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(center), icon, FALSE, FALSE, 0);

    GtkWidget* status_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_halign(status_row, GTK_ALIGN_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(status_row), "status-row");

    status_dot = gtk_label_new("●");
    gtk_style_context_add_class(gtk_widget_get_style_context(status_dot), "dot-idle");
    gtk_box_pack_start(GTK_BOX(status_row), status_dot, FALSE, FALSE, 0);

    status_label = gtk_label_new("Waiting for device...");
    gtk_style_context_add_class(gtk_widget_get_style_context(status_label), "status-idle");
    gtk_box_pack_start(GTK_BOX(status_row), status_label, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(center), status_row, FALSE, FALSE, 0);

    GtkWidget* btn_area = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_widget_set_halign(btn_area, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(center), btn_area, FALSE, FALSE, 0);

    transmit_button = gtk_button_new();
    GtkWidget* t_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(t_box, 4);
    gtk_widget_set_margin_end(t_box, 4);
    GtkWidget* t_icon = gtk_image_new_from_icon_name("media-record-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(t_box), t_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(t_box), gtk_label_new("Start streaming"), FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(transmit_button), t_box);
    gtk_style_context_add_class(gtk_widget_get_style_context(transmit_button), "transmit-btn");
    gtk_widget_set_sensitive(transmit_button, false);
    g_signal_connect(transmit_button, "clicked", G_CALLBACK(on_button_clicked), this);
    gtk_box_pack_start(GTK_BOX(btn_area), transmit_button, FALSE, FALSE, 0);

    cancel_button = gtk_button_new();
    GtkWidget* c_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_widget_set_margin_start(c_box, 4);
    gtk_widget_set_margin_end(c_box, 4);
    GtkWidget* c_icon = gtk_image_new_from_icon_name("media-playback-stop-symbolic", GTK_ICON_SIZE_BUTTON);
    gtk_box_pack_start(GTK_BOX(c_box), c_icon, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(c_box), gtk_label_new("Stop streaming"), FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(cancel_button), c_box);
    gtk_style_context_add_class(gtk_widget_get_style_context(cancel_button), "cancel-btn");
    g_signal_connect(cancel_button, "clicked", G_CALLBACK(on_cancel_clicked), this);
    gtk_box_pack_start(GTK_BOX(btn_area), cancel_button, FALSE, FALSE, 0);
    gtk_widget_set_no_show_all(cancel_button, TRUE);

    std::string version_text = "v" + std::to_string(APP_VERSION_MAJOR) + "." +
                               std::to_string(APP_VERSION_MINOR) + "." +
                               std::to_string(APP_VERSION_PATCH);
    GtkWidget* version_label = gtk_label_new(version_text.c_str());
    gtk_style_context_add_class(gtk_widget_get_style_context(version_label), "version-label");
    gtk_widget_set_halign(version_label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(version_label, 12);
    gtk_box_pack_start(GTK_BOX(center), version_label, FALSE, FALSE, 0);
}

static void reset_status_classes(GtkStyleContext* dot, GtkStyleContext* lbl) {
    gtk_style_context_remove_class(dot, "dot-idle");
    gtk_style_context_remove_class(dot, "dot-ready");
    gtk_style_context_remove_class(dot, "dot-active");
    gtk_style_context_remove_class(lbl, "status-idle");
    gtk_style_context_remove_class(lbl, "status-ready");
    gtk_style_context_remove_class(lbl, "status-active");
}

void Home::setTransmitButtonEnabled(bool enabled) {
    gtk_widget_set_sensitive(transmit_button, enabled);
}

void Home::setDeviceConnected(bool connected) {
    GtkStyleContext* dot_ctx = gtk_widget_get_style_context(status_dot);
    GtkStyleContext* lbl_ctx = gtk_widget_get_style_context(status_label);
    reset_status_classes(dot_ctx, lbl_ctx);

    if (connected) {
        gtk_label_set_text(GTK_LABEL(status_label), "Device connected");
        gtk_style_context_add_class(dot_ctx, "dot-ready");
        gtk_style_context_add_class(lbl_ctx, "status-ready");
    } else {
        gtk_label_set_text(GTK_LABEL(status_label), "Waiting for device...");
        gtk_style_context_add_class(dot_ctx, "dot-idle");
        gtk_style_context_add_class(lbl_ctx, "status-idle");
    }
}

void Home::setTransmitting(bool transmitting) {
    GtkStyleContext* dot_ctx = gtk_widget_get_style_context(status_dot);
    GtkStyleContext* lbl_ctx = gtk_widget_get_style_context(status_label);

    if (transmitting) {
        gtk_widget_hide(transmit_button);
        gtk_widget_show_all(gtk_bin_get_child(GTK_BIN(cancel_button)));
        gtk_widget_show(cancel_button);
        reset_status_classes(dot_ctx, lbl_ctx);
        gtk_label_set_text(GTK_LABEL(status_label), "Streaming...");
        gtk_style_context_add_class(dot_ctx, "dot-active");
        gtk_style_context_add_class(lbl_ctx, "status-active");
    } else {
        gtk_widget_show(transmit_button);
        gtk_widget_hide(cancel_button);
    }
}

void Home::show() {
    gtk_widget_show_all(window);
}
