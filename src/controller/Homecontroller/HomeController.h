#ifndef HOMECONTROLLER_H
#define HOMECONTROLLER_H

// Forward declaration
class Home;

class HomeController {
    public:
        HomeController(Home* view);
        ~HomeController();

        void initializeDBusConnection();
    private:
        Home* view_;
        std::unique_ptr<PortalManager> portal_manager_;
};

#endif