add_library(adb_monitor STATIC AdbMonitor.cpp)
target_include_directories(adb_monitor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
