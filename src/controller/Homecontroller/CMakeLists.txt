# CMakeLists.txt para el módulo HomeController

# Crear librería estática para HomeController
add_library(home_controller STATIC
    HomeController.cpp
    HomeController.h
)

# Incluir directorios necesarios para esta librería
target_include_directories(home_controller PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GTK_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
)

# Enlazar GTK y GStreamer con esta librería
target_link_libraries(home_controller PUBLIC
    ${GTK_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
)
