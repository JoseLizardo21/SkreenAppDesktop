add_library(webrtc_signaling STATIC WebRtcSignaling.cpp)
target_include_directories(webrtc_signaling PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
