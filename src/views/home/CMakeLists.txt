# CMakeLists.txt for the Home module

# Create a static library for Home
add_library(home_view STATIC
    Home.cpp
    Home.h
)

# Include directories required by this library
target_include_directories(home_view PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GTK_INCLUDE_DIRS}
)

# Link GTK against this library
target_link_libraries(home_view PUBLIC
    ${GTK_LIBRARIES}
)
