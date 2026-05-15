add_library(notify_server STATIC NotifyServer.cpp)
target_include_directories(notify_server PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
