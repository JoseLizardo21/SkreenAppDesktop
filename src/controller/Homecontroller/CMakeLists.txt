# CMakeLists.txt for the HomeController module

# Create a static library for HomeController
add_library(home_controller STATIC
    HomeController.cpp
    HomeController.h
)

# Include directories required by this library
target_include_directories(home_controller PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GTK_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
)

# Link GTK and GStreamer against this library
target_link_libraries(home_controller PUBLIC
    ${GTK_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    notify_server
    webrtc_signaling
    settings_view
    config_manager
)
