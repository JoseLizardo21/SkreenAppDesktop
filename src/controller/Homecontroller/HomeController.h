#ifndef HOMECONTROLLER_H
#define HOMECONTROLLER_H

#include <memory>
#include "../../services/system/portalmanager/PortalManager.h"
#include "../../services/media/gstreamermanager/GStreamerManager.h"

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
    std::unique_ptr<PortalManager> portal_manager_;
    std::unique_ptr<GStreamerManager> gstreamer_manager_;
};

#endif
