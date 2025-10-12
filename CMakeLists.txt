# Versão mínima do CMake necessária
cmake_minimum_required(VERSION 3.16)

# Nome do projeto
project(GamePadVirtual-Desktop VERSION 1.0 LANGUAGES CXX)

# Define a política da Runtime Library para todo o projeto.
# MultiThreaded significa /MT e MultiThreadedDebug significa /MTd
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")


# Define o padrão do C++ (Qt 6 requer no mínimo C++17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Configura o Qt
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Encontra os pacotes do Qt que vamos usar, incluindo Bluetooth e SerialPort
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Bluetooth SerialPort)

# Define o executável e lista TODOS os arquivos (incluindo os futuros do USB)
add_executable(GamePadVirtual-Desktop
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/communication/connection_manager.cpp
    src/communication/connection_manager.h
    src/communication/wifi_server.cpp
    src/communication/wifi_server.h
    src/communication/bluetooth_server.cpp
    src/communication/bluetooth_server.h
    src/communication/usb_monitor.cpp  # Arquivo que vamos criar
    src/communication/usb_monitor.h    # Arquivo que vamos criar
    src/virtual_gamepad/gamepad_manager.cpp
    src/virtual_gamepad/gamepad_manager.h
    src/protocol/gamepad_packet.h
)

# Adiciona o caminho para os cabeçalhos do ViGEmClient
target_include_directories(GamePadVirtual-Desktop PRIVATE
    "C:/Projetos/GamePadVirtual-Desktop/ViGEmClient-master/include"
)

target_link_libraries(GamePadVirtual-Desktop PRIVATE
    # ... Bibliotecas do Qt ...
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Network
    Qt6::Bluetooth
    Qt6::SerialPort

    # Biblioteca do Windows necessária para o ViGEm
    SetupAPI

    # Biblioteca ViGEmClient
    "C:/Projetos/GamePadVirtual-Desktop/ViGEmClient-master/lib/debug/x64/ViGEmClient.lib"
)

# Define que este é um aplicativo de janela
if(WIN32)
    set_target_properties(GamePadVirtual-Desktop PROPERTIES WIN32_EXECUTABLE ON)
endif()