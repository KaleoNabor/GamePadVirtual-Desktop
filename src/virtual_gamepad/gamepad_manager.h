#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef GAMEPAD_MANAGER_H
#define GAMEPAD_MANAGER_H

#include <QObject>
#include "../protocol/gamepad_packet.h"
#include <Windows.h>

// CORREÇÃO: Caminho correto para o header
#include "C:/Projetos/GamePadVirtual-Desktop/ViGEmClient-master/include/ViGEm/Client.h"

// As libs necessárias
#pragma comment(lib, "C:/Projetos/GamePadVirtual-Desktop/ViGEmClient-master/lib/debug/x64/ViGEmClient.lib")
#pragma comment(lib, "setupapi.lib")

#define MAX_PLAYERS 4

class GamepadManager : public QObject
{
    Q_OBJECT
public:
    explicit GamepadManager(QObject* parent = nullptr);
    ~GamepadManager();

    bool initialize();

public slots:
    void onPacketReceived(int playerIndex, const GamepadPacket& packet);

signals:
    void gamepadStateUpdated(int playerIndex, const GamepadPacket& packet);
    void playerConnected(int playerIndex, const QString& type);
    void playerDisconnected(int playerIndex);
    void vibrationCommandReady(int playerIndex, const QByteArray& command);

public:
    void onVibrationReceived(PVIGEM_TARGET target, unsigned char largeMotor, unsigned char smallMotor);

private:
    PVIGEM_CLIENT vigem_client;
    PVIGEM_TARGET virtual_gamepads[MAX_PLAYERS];
    XUSB_REPORT last_reports[MAX_PLAYERS];
};

#endif // GAMEPAD_MANAGER_H