# CMakeLists.txt para el módulo WebSocketClient

# Crear librería estática para WebSocketClient
add_library(websocket_client STATIC
    WebSocketClient.cpp
    WebSocketClient.h
)

# Incluir directorios necesarios para esta librería
target_include_directories(websocket_client PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${OPENSSL_INCLUDE_DIRS}  # NUEVO
)

# Enlazar con pthread y OpenSSL
target_link_libraries(websocket_client PUBLIC
    pthread
    ${OPENSSL_LIBRARIES}  # NUEVO
)