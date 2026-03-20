cmake_minimum_required(VERSION 3.10)

project(SkreenDesktop VERSION 1.0)

# Configurar estándar C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ========== Buscar paquetes ==========
find_package(PkgConfig REQUIRED)

# GTK, GStreamer, DBus
pkg_check_modules(GTK REQUIRED gtk+-3.0)
pkg_check_modules(GSTREAMER REQUIRED
    gstreamer-1.0
    gstreamer-app-1.0
    gstreamer-webrtc-1.0
    gstreamer-sdp-1.0
)
pkg_check_modules(DBUS REQUIRED dbus-1)

# ========== OpenSSL ==========
pkg_check_modules(OPENSSL REQUIRED openssl)

# ========== Threads ==========
find_package(Threads REQUIRED)

# ========== Incluir directorios de headers ==========
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${GTK_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${DBUS_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIRS}
)

# ========== Añadir directorios de librerías ==========
link_directories(
    ${GTK_LIBRARY_DIRS}
    ${GSTREAMER_LIBRARY_DIRS}
    ${OPENSSL_LIBRARY_DIRS}
)

# ========== Definiciones adicionales ==========
add_definitions(
    ${GTK_CFLAGS_OTHER}
    ${GSTREAMER_CFLAGS_OTHER}
)

# ========== Añadir subdirectorios ==========
add_subdirectory(src)

# ========== Crear ejecutable principal ==========
add_executable(skreen_desktop main.cpp)

# ========== Enlazar con las librerías ==========
target_link_libraries(skreen_desktop
    # Módulos internos
    home_view
    gstreamer_manager
    home_controller
    portal_manager
    websocket_client
    
    # Librerías externas
    ${GTK_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${DBUS_LIBRARIES}
    ${OPENSSL_LIBRARIES}
    Threads::Threads
)