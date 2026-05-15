#ifndef HOMECONTROLLER_H
#define HOMECONTROLLER_H

#include <memory>
#include <atomic>
#include <thread>
#include "../../services/system/portalmanager/PortalManager.h"
#include "../../services/media/gstreamermanager/GStreamerManager.h"
#include "../../services/input/InputServer.h"
#include "../../services/adb/AdbMonitor.h"
#include "../../services/notify/NotifyServer.h"

class Home;

class HomeController {
public:
    HomeController(Home* home);
    ~HomeController();
    void handleRequestPermissions();
    void onPortalComplete(const std::string& session_handle, uint32_t node_id, int fd);
    void onGStreamerError(const std::string& message);
    void handleStopCapture();
private:
    Home* view_;
    bool device_connected_ = false;
    std::unique_ptr<PortalManager> portal_manager_;
    std::unique_ptr<GStreamerManager> gstreamer_manager_;
    std::unique_ptr<InputServer> input_server_;
    std::unique_ptr<AdbMonitor> adb_monitor_;
    std::unique_ptr<NotifyServer> notify_server_;
    std::thread stop_thread_;
};

#endif
