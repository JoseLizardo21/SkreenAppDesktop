# CMakeLists.txt for the gstreamermanager module

# Create a static library for gstreamermanager
add_library(gstreamer_manager STATIC
    GStreamerManager.cpp
    GStreamerManager.h
)

# Include directories required by this library
target_include_directories(gstreamer_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GTK_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${NICE_INCLUDE_DIRS}
)

# Link GTK, GStreamer and libnice against this library
target_link_libraries(gstreamer_manager PUBLIC
    ${GTK_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${NICE_LIBRARIES}
)
