# CMakeLists.txt para el módulo MonitorControl

add_library(monitor_control STATIC
    MonitorControl.cpp
    MonitorControl.h
)

target_include_directories(monitor_control PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
