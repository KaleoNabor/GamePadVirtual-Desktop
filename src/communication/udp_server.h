#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QHash>
#include <QTimer>
#include "../protocol/gamepad_packet.h"

#define MAX_PLAYERS 4

class UdpServer : public QObject
{
    Q_OBJECT

public:
    explicit UdpServer(QObject* parent = nullptr);
    ~UdpServer();

    // Fun��o para envio de dados para jogador espec�fico
    bool sendToPlayer(int playerIndex, const QByteArray& data);

public slots:
    // In�cio e parada do servidor UDP
    void startServer(quint16 port);
    void stopServer();

private slots:
    // Slot para processamento de datagramas pendentes
    void processPendingDatagrams();

signals:
    // Sinais para comunica��o externa
    void packetReceived(int playerIndex, const GamepadPacket& packet);
    void playerConnected(int playerIndex, const QString& type);
    void playerDisconnected(int playerIndex);
    void logMessage(const QString& message);

private:
    // Estrutura para identifica��o de cliente
    struct ClientId {
        QHostAddress address;
        quint16 port;
        bool operator==(const ClientId& other) const {
            return address == other.address && port == other.port;
        }
    };
    friend uint qHash(const ClientId& key, uint seed) {
        return qHash(key.address.toString(), seed) ^ key.port;
    }

    // Fun��es auxiliares
    int findEmptySlot() const;
    void handlePlayerDisconnect(int playerIndex);

    // Membros da classe
    QUdpSocket* m_udpSocket;
    QTimer* m_processingTimer;
    QHash<ClientId, int> m_clientPlayerMap;
    QHash<int, ClientId> m_playerClientMap;
    bool m_playerSlots[MAX_PLAYERS];
};

#endif