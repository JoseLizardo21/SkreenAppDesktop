add_library(config_manager ConfigManager.cpp)
target_include_directories(config_manager PUBLIC ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)
