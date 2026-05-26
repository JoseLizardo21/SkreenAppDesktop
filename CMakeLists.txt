cmake_minimum_required(VERSION 3.10)

project(SkreenDesktop VERSION 0.1.0)

# Versión con git hash para builds de desarrollo (ej: v0.1.0-3-gabcd123)
execute_process(
  COMMAND git describe --tags --always --dirty
  WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
  OUTPUT_VARIABLE GIT_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
  ERROR_QUIET
)
if(GIT_VERSION STREQUAL "")
  set(APP_VERSION "${PROJECT_VERSION}")
else()
  set(APP_VERSION "${GIT_VERSION}")
endif()

configure_file(
  ${CMAKE_SOURCE_DIR}/include/version.h.in
  ${CMAKE_BINARY_DIR}/version.h
)

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
    gstreamer-video-1.0
)
pkg_check_modules(DBUS REQUIRED dbus-1)

# ========== Threads ==========
find_package(Threads REQUIRED)

# ========== Incluir directorios de headers ==========
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}
    ${GTK_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${DBUS_INCLUDE_DIRS}
)

# ========== Añadir directorios de librerías ==========
link_directories(
    ${GTK_LIBRARY_DIRS}
    ${GSTREAMER_LIBRARY_DIRS}
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
    home_view
    settings_view
    gstreamer_manager
    home_controller
    portal_manager
    input_server
    adb_monitor
    config_manager
    ${GTK_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${DBUS_LIBRARIES}
    Threads::Threads
)