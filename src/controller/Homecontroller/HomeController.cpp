#include "HomeController.h"
#include "../../services/system/portalmanager/PortalManager.h"
#include "../views/home/Home.h"
#include <memory>

HomeController::HomeController(Home* view)
    : view_(view) {
    portal_manager_ = std::make_unique<PortalManager>();
}