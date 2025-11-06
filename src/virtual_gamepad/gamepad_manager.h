// gamepad_manager.h

#ifndef GAMEPAD_MANAGER_H
#define GAMEPAD_MANAGER_H

#include <QObject>
#include <QTimer>
#include <QAtomicInt>
#include <QUdpSocket>
#include <QHostAddress>
#include "../protocol/gamepad_packet.h"

#define _WINSOCKAPI_
#include <Windows.h>
#undef _WINSOCKAPI_
#include "ViGEm/Client.h"

using VigemClient = PVIGEM_CLIENT;
using VigemTarget = PVIGEM_TARGET;

#define MAX_PLAYERS 8

class GamepadManager : public QObject
{
    Q_OBJECT

public:
    explicit GamepadManager(QObject* parent = nullptr);
    ~GamepadManager();

    bool initialize();
    void shutdown();

public slots:
    void onPacketReceived(int playerIndex, const GamepadPacket& packet);
    void playerConnected(int playerIndex, const QString& type);
    void playerDisconnected(int playerIndex);
    void testVibration(int playerIndex);

private slots:
    void processLatestPackets();

signals:
    void gamepadStateUpdated(int playerIndex, const GamepadPacket& packet);
    void playerConnectedSignal(int playerIndex, const QString& type);
    void playerDisconnectedSignal(int playerIndex);
    void vibrationCommandReady(int playerIndex, const QByteArray& command);

private:
    void cleanupGamepad(int playerIndex);
    void handleX360Vibration(int playerIndex, UCHAR largeMotor, UCHAR smallMotor);

    VigemClient m_client;
    VigemTarget m_targets[MAX_PLAYERS];
    bool m_connected[MAX_PLAYERS];
    QTimer* m_processingTimer;
    GamepadPacket m_latestPackets[MAX_PLAYERS];
    QAtomicInt m_dirtyFlags[MAX_PLAYERS];

    QUdpSocket* m_cemuhookSocket;
    QHostAddress m_cemuhookHost;
    quint16 m_cemuhookPort;

    static void CALLBACK x360NotificationCallback(
        VigemClient Client, VigemTarget Target, UCHAR LargeMotor,
        UCHAR SmallMotor, UCHAR LedNumber, LPVOID UserData);
};

#endif