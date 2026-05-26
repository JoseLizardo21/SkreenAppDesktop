add_library(settings_view Settings.cpp)
target_include_directories(settings_view PUBLIC
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/include
    ${GTK_INCLUDE_DIRS}
)
target_link_libraries(settings_view ${GTK_LIBRARIES})
