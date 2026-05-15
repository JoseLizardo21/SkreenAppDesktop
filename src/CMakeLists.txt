# CMakeLists.txt para el directorio src

# Añadir subdirectorios de controladores
add_subdirectory(controller/Homecontroller)

# Añadir subdirectorios de servicios
add_subdirectory(services/system/portalmanager)
add_subdirectory(services/websocketclient)

# Añadir subdirectorios de vistas
add_subdirectory(views/home)
add_subdirectory(services/media/gstreamermanager)
add_subdirectory(services/input)
add_subdirectory(services/adb)
add_subdirectory(services/notify)
