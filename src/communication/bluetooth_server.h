#ifndef BLUETOOTH_SERVER_H
#define BLUETOOTH_SERVER_H

#include <QObject>
#include <QBluetoothServer>
#include <QBluetoothSocket>
#include <QList>
#include "../protocol/gamepad_packet.h"

// Define o número máximo de jogadores conectados simultaneamente
#define MAX_PLAYERS 8

// Classe principal do servidor Bluetooth para gerenciar conexões de jogadores
class BluetoothServer : public QObject
{
    Q_OBJECT

public:
    explicit BluetoothServer(QObject* parent = nullptr);
    ~BluetoothServer();

    // --- SEÇÃO: MÉTODOS PÚBLICOS ---

    // Função para envio de dados para jogador específico
    bool sendToPlayer(int playerIndex, const QByteArray& data);

public slots:
    // --- SEÇÃO: SLOTS PÚBLICOS ---

    // Início e parada do servidor Bluetooth
    void startServer();
    void stopServer();

    // --- MODIFICAÇÃO ADICIONADA (REQ 2 FIX) ---
    // Força a desconexão de um jogador específico via Bluetooth
    void forceDisconnectPlayer(int playerIndex);

private slots:
    // --- SEÇÃO: SLOTS PRIVADOS ---

    // Slots para eventos de conexão Bluetooth
    void clientConnected();      // Gerencia novas conexões de clientes
    void readSocket();           // Processa dados recebidos dos sockets
    void clientDisconnected();   // Trata desconexões de clientes

signals:
    // --- SEÇÃO: SINAIS ---

    // Sinais para comunicação externa
    void packetReceived(int playerIndex, const GamepadPacket& packet);  // Pacote de gamepad recebido
    void playerConnected(int playerIndex, const QString& type);         // Novo jogador conectado
    void playerDisconnected(int playerIndex);                           // Jogador desconectado
    void logMessage(const QString& message);                            // Mensagens de log

private:
    // --- SEÇÃO: MÉTODOS PRIVADOS ---

    // Busca de slot vazio para jogador
    int findEmptySlot() const;

    // --- SEÇÃO: MEMBROS PRIVADOS ---

    // Servidor Bluetooth principal (RFCOMM)
    QBluetoothServer* m_btServer;
    // Lista de sockets de clientes conectados
    QList<QBluetoothSocket*> m_clientSockets;
    // Mapeamento de sockets para índices de jogador
    QHash<QBluetoothSocket*, int> m_socketPlayerMap;
    // Array de slots de jogador ocupados (controle de disponibilidade)
    bool m_playerSlots[MAX_PLAYERS];
};

#endif