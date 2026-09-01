add_library(network_info STATIC NetworkInfo.cpp)
target_include_directories(network_info PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
