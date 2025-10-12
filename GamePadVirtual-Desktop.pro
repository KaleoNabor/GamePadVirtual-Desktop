QT += core gui widgets network bluetooth

CONFIG += c++17
CONFIG -= app_bundle

TARGET = GamePadVirtual-Desktop
TEMPLATE = app

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/communication/connection_manager.cpp \
    src/communication/wifi_server.cpp \
    src/communication/bluetooth_server.cpp \
    src/virtual_gamepad/gamepad_manager.cpp

HEADERS += \
    src/mainwindow.h \
    src/communication/connection_manager.h \
    src/communication/wifi_server.h \
    src/communication/bluetooth_server.h \
    src/virtual_gamepad/gamepad_manager.h \
    src/protocol/gamepad_packet.h

msvc: QMAKE_CXXFLAGS = $$replace(QMAKE_CXXFLAGS, "/MD", "/MT")

win32: LIBS += SetupAPI.lib

INCLUDEPATH += "C:/Projetos/GamePadVirtual-Desktop/ViGEmClient-master/include"
LIBS += -L"C:/Projetos/GamePadVirtual-Desktop/ViGEmClient-master/lib/debug/x64" -lViGEmClient