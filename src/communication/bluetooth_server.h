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

    // =========================================================================
    // ADICIONE A DECLARAÇÃO DA NOVA FUNÇÃO AQUI
    // =========================================================================
    bool sendToPlayer(int playerIndex, const QByteArray& data);

public slots:
    void startServer();
    void stopServer();

private slots:
    void clientConnected();
    void readSocket();
    void clientDisconnected();

signals:
    void packetReceived(int playerIndex, const GamepadPacket& packet);
    void playerConnected(int playerIndex, const QString& type);
    void playerDisconnected(int playerIndex);
    void logMessage(const QString& message);

private:
    int findEmptySlot() const;

    QBluetoothServer* m_btServer;
    QList<QBluetoothSocket*> m_clientSockets;
    QHash<QBluetoothSocket*, int> m_socketPlayerMap;
    bool m_playerSlots[MAX_PLAYERS];
};

#endif // BLUETOOTH_SERVER_H