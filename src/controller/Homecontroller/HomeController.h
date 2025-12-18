#ifndef HOMECONTROLLER_H
#define HOMECONTROLLER_H

#include <memory>
#include "../../services/system/portalmanager/PortalManager.h"

// Forward declaration
class Home;

class HomeController {
    public:
        HomeController(Home* view);
        ~HomeController();
        void handleRequestPermissions();
        void initializeDBusConnection();
    private:
        Home* view_;
        std::unique_ptr<PortalManager> portal_manager_;
};

#endif