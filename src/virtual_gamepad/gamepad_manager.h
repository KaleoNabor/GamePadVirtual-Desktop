// gamepad_manager.h - CORREÇÕES DE SINTAXE

#ifndef GAMEPAD_MANAGER_H
#define GAMEPAD_MANAGER_H

#include <QObject>
#include <QTimer>
#include <QAtomicInt>
#include <QUdpSocket>
#include <QHostAddress>
#include <QElapsedTimer>
#include "../protocol/gamepad_packet.h"
#include "../controller_types.h"

// CORREÇÃO: Use includes padrão do Windows
#include <Windows.h>
#include <ViGEm/Client.h>

// CORREÇÃO: Use os tipos ViGEm diretamente
using VigemClient = PVIGEM_CLIENT;
using VigemTarget = PVIGEM_TARGET;

class GamepadManager : public QObject
{
    Q_OBJECT

public:
    explicit GamepadManager(QObject* parent = nullptr);
    ~GamepadManager();

    bool initialize();
    void shutdown();
    void printServerStatus();

public slots:
    void onPacketReceived(int playerIndex, const GamepadPacket& packet);
    void playerConnected(int playerIndex, const QString& type);
    void playerDisconnected(int playerIndex);
    void testVibration(int playerIndex);
    void onControllerTypeChanged(int playerIndex, int typeIndex);

private slots:
    void processLatestPackets();
    void readPendingCemuhookDatagrams();

signals:
    void gamepadStateUpdated(int playerIndex, const GamepadPacket& packet);
    void playerConnectedSignal(int playerIndex, const QString& type);
    void playerDisconnectedSignal(int playerIndex);
    void vibrationCommandReady(int playerIndex, const QByteArray& command);
    void dsuClientConnected(const QString& address, quint16 port);
    void dsuClientDisconnected();

private:
    void createGamepad(int playerIndex);
    void cleanupGamepad(int playerIndex);
    void handleX360Vibration(int playerIndex, UCHAR largeMotor, UCHAR smallMotor);
    void handleDS4Vibration(int playerIndex, UCHAR largeMotor, UCHAR smallMotor);

    // CORREÇÃO: Use tipos ViGEm corretos
    VigemClient m_client;
    VigemTarget m_targets[MAX_PLAYERS];
    bool m_connected[MAX_PLAYERS];
    QTimer* m_processingTimer;
    GamepadPacket m_latestPackets[MAX_PLAYERS];
    QAtomicInt m_dirtyFlags[MAX_PLAYERS];
    ControllerType m_controllerTypes[MAX_PLAYERS];

    QUdpSocket* m_cemuhookSocket;
    quint16 m_cemuhookPort;
    QHostAddress m_cemuhookClientAddress;
    quint16 m_cemuhookClientPort;
    bool m_cemuhookClientSubscribed;
    QElapsedTimer m_cemuhookClientTimer;
    quint32 m_dsuPacketCounter[MAX_PLAYERS];
    uint m_dsuLastKeepAlive[MAX_PLAYERS];

    // CORREÇÃO: Callbacks com tipos corretos
    static void CALLBACK x360NotificationCallback(
        PVIGEM_CLIENT Client, PVIGEM_TARGET Target, UCHAR LargeMotor,
        UCHAR SmallMotor, UCHAR LedNumber, PVOID UserData);

    static void CALLBACK ds4NotificationCallback(
        PVIGEM_CLIENT Client, PVIGEM_TARGET Target, UCHAR LargeMotor,
        UCHAR SmallMotor, DS4_LIGHTBAR_COLOR LightbarColor, PVOID UserData);
};

#endif