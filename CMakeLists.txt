cmake_minimum_required(VERSION 3.10)

project(SkreenDesktop VERSION 1.0)

# Configurar estándar C++
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Buscar GTK
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK REQUIRED gtk+-3.0)

# Incluir directorios de headers
include_directories(
    ${CMAKE_SOURCE_DIR}/src
    ${GTK_INCLUDE_DIRS}
)

# Añadir directorios de librerías
link_directories(${GTK_LIBRARY_DIRS})

# Definiciones adicionales
add_definitions(${GTK_CFLAGS_OTHER})

# Añadir subdirectorios
add_subdirectory(src)

# Crear ejecutable principal
add_executable(skreen_desktop main.cpp)

# Enlazar con las librerías de los módulos
target_link_libraries(skreen_desktop
    home_view
    ${GTK_LIBRARIES}
)