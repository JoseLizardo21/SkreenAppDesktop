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
#include "../../services/media/webrtcsignaling/WebRtcSignaling.h"
#include "config/ConfigManager.h"

class Home;

class HomeController {
public:
    HomeController(Home* home);
    ~HomeController();
    void handleRequestPermissions();
    void handleOpenSettings();
    void onPortalComplete(const std::string& session_handle, uint32_t node_id, int fd);
    void onGStreamerError(const std::string& message);
    void handleStopCapture();
private:
    Home* view_;
    bool device_connected_ = false;
    StreamConfig config_;
    ConfigManager config_manager_;
    // Destrucción en orden inverso: gstreamer_manager_ debe sobrevivir a
    // portal_manager_ porque el worker thread del portal puede llamar a
    // gm->restartPipeline() hasta que su join() retorne en ~PortalManager().
    std::unique_ptr<GStreamerManager> gstreamer_manager_;
    std::unique_ptr<PortalManager> portal_manager_;
    std::unique_ptr<InputServer> input_server_;
    std::unique_ptr<AdbMonitor> adb_monitor_;
    std::unique_ptr<NotifyServer> notify_server_;
    std::unique_ptr<WebRtcSignaling> webrtc_signaling_;
    // Flag compartido con los lambdas de reconexión para abortar restartPipeline
    // si el usuario paró la sesión antes de que el fd request completara.
    std::shared_ptr<std::atomic<bool>> session_active_{std::make_shared<std::atomic<bool>>(false)};
    std::thread stop_thread_;
};

#endif
