#ifndef HOMECONTROLLER_H
#define HOMECONTROLLER_H

#include <memory>
#include <atomic>
#include <thread>
#include "../../services/system/portalmanager/PortalManager.h"
#include "../../services/system/monitorcontrol/MonitorControl.h"
#include "../../services/media/gstreamermanager/GStreamerManager.h"
#include "../../services/input/InputServer.h"
#include "../../services/adb/AdbMonitor.h"
#include "../../services/notify/NotifyServer.h"
#include "../../services/media/webrtcsignaling/WebRtcSignaling.h"
#include "../../services/network/NetworkInfo.h"
#include "config/ConfigManager.h"
#include "config/ConnectionMode.h"

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
    bool handleMonitorToggle(bool enabled);
    void handleConnectionModeChanged(ConnectionMode mode);
private:
    Home* view_;
    bool device_connected_ = false;
    ConnectionMode connection_mode_{ConnectionMode::Cable};
    StreamConfig config_;
    ConfigManager config_manager_;
    std::unique_ptr<MonitorControl> monitor_control_;
    // Destroyed in reverse order: gstreamer_manager_ must outlive
    // portal_manager_ because the portal worker thread may call
    // gm->restartPipeline() until its join() returns in ~PortalManager().
    std::unique_ptr<GStreamerManager> gstreamer_manager_;
    std::unique_ptr<PortalManager> portal_manager_;
    std::unique_ptr<InputServer> input_server_;
    std::unique_ptr<AdbMonitor> adb_monitor_;
    std::unique_ptr<NotifyServer> notify_server_;
    std::unique_ptr<WebRtcSignaling> webrtc_signaling_;
    // Flag shared with the reconnection lambdas to abort restartPipeline if the
    // user stopped the session before the fd request completed.
    std::shared_ptr<std::atomic<bool>> session_active_{std::make_shared<std::atomic<bool>>(false)};
    std::thread stop_thread_;
};

#endif
