#ifndef GAMEPAD_MANAGER_H
#define GAMEPAD_MANAGER_H

#include <QObject>
#include <QTimer>
#include <QAtomicInt>
#include "../protocol/gamepad_packet.h"

// Inclusão da biblioteca ViGEm com definições corretas
#define _WINSOCKAPI_
#include <Windows.h>
#undef _WINSOCKAPI_
#include "ViGEm/Client.h"

// Definições de tipos para ViGEm
using VigemClient = PVIGEM_CLIENT;
using VigemTarget = PVIGEM_TARGET;

#define MAX_PLAYERS 8

class GamepadManager : public QObject
{
    Q_OBJECT

public:
    explicit GamepadManager(QObject* parent = nullptr);
    ~GamepadManager();

    // Inicialização e encerramento do gerenciador
    bool initialize();
    void shutdown();

public slots:
    // Slot para receber pacotes de dados dos gamepads
    void onPacketReceived(int playerIndex, const GamepadPacket& packet);
    // Slots para gerenciar conexões de jogadores
    void playerConnected(int playerIndex, const QString& type);
    void playerDisconnected(int playerIndex);

    // +++ ADICIONE ESTE NOVO SLOT +++
    void testVibration(int playerIndex);

private slots:
    // Slot do loop de processamento principal
    void processLatestPackets();

signals:
    // Sinais para comunicação com a interface
    void gamepadStateUpdated(int playerIndex, const GamepadPacket& packet);
    void playerConnectedSignal(int playerIndex, const QString& type);
    void playerDisconnectedSignal(int playerIndex);
    void vibrationCommandReady(int playerIndex, const QByteArray& command);

private:
    // Limpeza de gamepad individual
    void cleanupGamepad(int playerIndex);
    // Processamento de comandos de vibração
    void handleVibration(int playerIndex, UCHAR largeMotor, UCHAR smallMotor);

    // Cliente ViGEm
    VigemClient m_client;
    // Array de targets (gamepads virtuais)
    VigemTarget m_targets[MAX_PLAYERS];
    // Estados de conexão dos jogadores
    bool m_connected[MAX_PLAYERS];

    // Timer para processamento em loop
    QTimer* m_processingTimer;
    // Pacotes mais recentes recebidos
    GamepadPacket m_latestPackets[MAX_PLAYERS];
    // Flags atômicas para controle de dados novos
    QAtomicInt m_dirtyFlags[MAX_PLAYERS];

    // Callback estático para notificações de vibração
    static void CALLBACK x360NotificationCallback(
        VigemClient Client, VigemTarget Target, UCHAR LargeMotor,
        UCHAR SmallMotor, UCHAR LedNumber, LPVOID UserData);
};

#endif