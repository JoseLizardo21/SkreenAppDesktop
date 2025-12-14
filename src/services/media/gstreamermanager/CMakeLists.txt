# CMakeLists.txt para el módulo gstreamermanager

# Crear librería estática para gstreamermanager
add_library(gstreamer_manager STATIC
    GStreamerManager.cpp
    GStreamerManager.h
)

# Incluir directorios necesarios para esta librería
target_include_directories(gstreamer_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GTK_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
)

# Enlazar GTK y GStreamer con esta librería
target_link_libraries(gstreamer_manager PUBLIC
    ${GTK_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
)
