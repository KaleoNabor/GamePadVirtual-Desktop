#ifndef GAMEPAD_MANAGER_H
#define GAMEPAD_MANAGER_H

#include <QObject>
#include <QTimer>
#include <QAtomicInt>
#include "../protocol/gamepad_packet.h"

// Incluir ViGEm de forma correta
#define _WINSOCKAPI_
#include <Windows.h>
#undef _WINSOCKAPI_
#include "ViGEm/Client.h"

// Usar as definições originais do ViGEm
using VigemClient = PVIGEM_CLIENT;
using VigemTarget = PVIGEM_TARGET;

#define MAX_PLAYERS 4

class GamepadManager : public QObject
{
    Q_OBJECT

public:
    explicit GamepadManager(QObject* parent = nullptr);
    ~GamepadManager();

    bool initialize();
    void shutdown();

public slots:
    // Slot rápido para receber dados da thread de rede
    void onPacketReceived(int playerIndex, const GamepadPacket& packet);
    // Slots para gerenciar conexões
    void playerConnected(int playerIndex, const QString& type);
    void playerDisconnected(int playerIndex);

private slots:
    // Slot do "game loop" para processar os dados na thread principal
    void processLatestPackets();

signals:
    // Sinais para a MainWindow
    void gamepadStateUpdated(int playerIndex, const GamepadPacket& packet);
    void playerConnectedSignal(int playerIndex, const QString& type);
    void playerDisconnectedSignal(int playerIndex);
    void vibrationCommandReady(int playerIndex, const QByteArray& command);

private:
    void cleanupGamepad(int playerIndex);
    void handleVibration(int playerIndex, UCHAR largeMotor, UCHAR smallMotor);

    // Membros para o ViGEm
    VigemClient m_client;
    VigemTarget m_targets[MAX_PLAYERS];
    bool m_connected[MAX_PLAYERS];

    // Membros para a lógica de processamento desacoplado
    QTimer* m_processingTimer;
    GamepadPacket m_latestPackets[MAX_PLAYERS];
    QAtomicInt m_dirtyFlags[MAX_PLAYERS]; // Flags para marcar dados novos

    // Callback estático para a vibração
    static void CALLBACK x360NotificationCallback(
        VigemClient Client, VigemTarget Target, UCHAR LargeMotor,
        UCHAR SmallMotor, UCHAR LedNumber, LPVOID UserData);
};

#endif // GAMEPAD_MANAGER_H