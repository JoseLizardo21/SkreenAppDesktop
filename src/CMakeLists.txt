# CMakeLists.txt for the src directory

# Config
add_subdirectory(config)

# Add controller subdirectories
add_subdirectory(controller/Homecontroller)

# Add service subdirectories
add_subdirectory(services/system/portalmanager)
add_subdirectory(services/system/monitorcontrol)
add_subdirectory(services/network)

# Add view subdirectories
add_subdirectory(views/home)
add_subdirectory(views/settings)
add_subdirectory(services/media/gstreamermanager)
add_subdirectory(services/input)
add_subdirectory(services/adb)
add_subdirectory(services/notify)
add_subdirectory(services/media/webrtcsignaling)
