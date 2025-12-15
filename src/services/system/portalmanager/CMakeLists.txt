# CMakeLists.txt para el módulo PortalManager

# Crear librería estática para PortalManager
add_library(portal_manager STATIC
    PortalManager.cpp
    PortalManager.h
)

target_include_directories(portal_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GTK_INCLUDE_DIRS}
)

# Enlazar GTK con esta librería
target_link_libraries(portal_manager PUBLIC
    ${GTK_LIBRARIES}
)
