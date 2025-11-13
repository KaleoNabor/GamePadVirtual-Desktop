#ifndef NETWORK_SERVER_H
#define NETWORK_SERVER_H

#include <QObject>
#include <QList>
#include <QHash>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUdpSocket>
#include <QHostAddress>
#include "../controller_types.h"
#include "../protocol/gamepad_packet.h"


#define CONTROL_PORT_TCP 42000 // TCP para conexão/desconexão
#define DATA_PORT_UDP 42001    // UDP para pacotes de gamepad
#define DISCOVERY_PORT 27016   // UDP para descoberta de servidores

class NetworkServer : public QObject
{
    Q_OBJECT

public:
    explicit NetworkServer(QObject* parent = nullptr);
    ~NetworkServer();

public slots:
    void startServer();
    void stopServer();
    void forceDisconnectPlayer(int playerIndex);
    void sendVibration(int playerIndex, const QByteArray& command);

private slots:
    // Canal de controle TCP
    void newTcpConnection();
    void tcpClientDisconnected();
    void readTcpSocket();

    // CORREÇÃO: Slot de erro que estava faltando
    void tcpSocketError(QAbstractSocket::SocketError socketError);

    // Canal de dados UDP
    void readUdpDatagrams();

    // Canal de descoberta UDP
    void readDiscoveryDatagrams();
signals:
    void packetReceived(int playerIndex, const GamepadPacket& packet);
    void playerConnected(int playerIndex, const QString& type);
    void playerDisconnected(int playerIndex);
    void logMessage(const QString& message);

private:
    int findEmptySlot() const;

    // Servidores de rede
    QTcpServer* m_tcpServer;
    QUdpSocket* m_udpSocket;
    QUdpSocket* m_discoverySocket;

    // Gerenciamento de jogadores
    bool m_playerSlots[MAX_PLAYERS];
    QHash<QTcpSocket*, int> m_socketPlayerMap;
    QHash<QHostAddress, int> m_ipPlayerMap;
    QHash<int, QHostAddress> m_playerIpMap;
    QHash<int, quint16> m_playerUdpPortMap;
};

#endif // NETWORK_SERVER_H