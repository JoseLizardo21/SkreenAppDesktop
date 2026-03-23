add_library(input_server STATIC InputServer.cpp)
target_include_directories(input_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
