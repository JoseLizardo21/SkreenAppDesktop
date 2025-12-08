# CMakeLists.txt para el módulo Home

# Crear librería estática para Home
add_library(home_view STATIC
    Home.cpp
    Home.h
)

# Incluir directorios necesarios para esta librería
target_include_directories(home_view PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${GTK_INCLUDE_DIRS}
)

# Enlazar GTK con esta librería
target_link_libraries(home_view PUBLIC
    ${GTK_LIBRARIES}
)
