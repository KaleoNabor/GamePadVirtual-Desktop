#ifndef BLUETOOTH_SERVER_H
#define BLUETOOTH_SERVER_H

#include <QObject>
#include <QBluetoothServer>
#include <QBluetoothSocket>
#include <QList>
#include "../protocol/gamepad_packet.h"

#define MAX_PLAYERS 4

class BluetoothServer : public QObject
{
    Q_OBJECT

public:
    explicit BluetoothServer(QObject* parent = nullptr);
    ~BluetoothServer();

    // Função para envio de dados para jogador específico
    bool sendToPlayer(int playerIndex, const QByteArray& data);

public slots:
    // Início e parada do servidor Bluetooth
    void startServer();
    void stopServer();

private slots:
    // Slots para eventos de conexão Bluetooth
    void clientConnected();
    void readSocket();
    void clientDisconnected();

signals:
    // Sinais para comunicação externa
    void packetReceived(int playerIndex, const GamepadPacket& packet);
    void playerConnected(int playerIndex, const QString& type);
    void playerDisconnected(int playerIndex);
    void logMessage(const QString& message);

private:
    // Busca de slot vazio para jogador
    int findEmptySlot() const;

    // Servidor Bluetooth principal
    QBluetoothServer* m_btServer;
    // Lista de sockets de clientes conectados
    QList<QBluetoothSocket*> m_clientSockets;
    // Mapeamento de sockets para índices de jogador
    QHash<QBluetoothSocket*, int> m_socketPlayerMap;
    // Array de slots de jogador ocupados
    bool m_playerSlots[MAX_PLAYERS];
};

#endif