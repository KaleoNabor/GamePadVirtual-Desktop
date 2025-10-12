#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QHash>
#include <QTimer> // <<< ADICIONE O INCLUDE DO QTIMER
#include "../protocol/gamepad_packet.h"

#define MAX_PLAYERS 4

class UdpServer : public QObject
{
    Q_OBJECT

public:
    explicit UdpServer(QObject* parent = nullptr);
    ~UdpServer();

    bool sendToPlayer(int playerIndex, const QByteArray& data);

public slots:
    void startServer(quint16 port);
    void stopServer();

private slots:
    // <<< RENOMEADO DE readPendingDatagrams PARA processPendingDatagrams
    void processPendingDatagrams();

signals:
    void packetReceived(int playerIndex, const GamepadPacket& packet);
    void playerConnected(int playerIndex, const QString& type);
    void playerDisconnected(int playerIndex);
    void logMessage(const QString& message);

private:
    // (A estrutura ClientId e o qHash permanecem os mesmos)
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

    int findEmptySlot() const;
    void handlePlayerDisconnect(int playerIndex);

    QUdpSocket* m_udpSocket;
    QTimer* m_processingTimer; // <<< ADICIONE O PONTEIRO PARA O TIMER
    QHash<ClientId, int> m_clientPlayerMap;
    QHash<int, ClientId> m_playerClientMap;
    bool m_playerSlots[MAX_PLAYERS];
};

#endif // UDP_SERVER_H