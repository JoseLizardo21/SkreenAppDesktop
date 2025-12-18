#include "HomeController.h"
#include "../../views/home/Home.h"
#include "../../services/system/portalmanager/PortalManager.h"
#include <memory>

HomeController::HomeController(Home* view)
    : view_(view) {
    portal_manager_ = std::make_unique<PortalManager>();
    view_->setOnRequestPermissionsCallback(
        [this]() { handleRequestPermissions(); });
}

HomeController::~HomeController() {
    // handleStopCapture();
}

void HomeController::handleRequestPermissions() {

    if (portal_manager_) {
        portal_manager_->startAsync();
    }
}