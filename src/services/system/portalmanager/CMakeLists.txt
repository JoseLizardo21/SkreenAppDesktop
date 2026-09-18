# CMakeLists.txt for the PortalManager module

# Create a static library for PortalManager
add_library(portal_manager STATIC
    PortalManager.cpp
    PortalManager.h
)

target_include_directories(portal_manager PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GTK_INCLUDE_DIRS}
)

# Link GTK against this library
target_link_libraries(portal_manager PUBLIC
    ${GTK_LIBRARIES}
)
