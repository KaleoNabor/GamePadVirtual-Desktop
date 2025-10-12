#ifndef TCP_SERVER_H
#define TCP_SERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QList>
#include <QHash>
#include "../protocol/gamepad_packet.h"

#define MAX_PLAYERS 4

class TcpServer : public QObject
{
    Q_OBJECT

public:
    explicit TcpServer(QObject* parent = nullptr);
    ~TcpServer();

public slots:
    void startServer(quint16 port);
    void stopServer();

private slots:
    void clientConnected();
    void readSocket();
    void clientDisconnected();

signals:
    void packetReceived(int playerIndex, const GamepadPacket& packet);
    void playerConnected(int playerIndex, const QString& type);
    void playerDisconnected(int playerIndex);

private:
    int findEmptySlot() const;

    QTcpServer* m_tcpServer;
    QList<QTcpSocket*> m_clientSockets;
    QHash<QTcpSocket*, int> m_socketPlayerMap;
    bool m_playerSlots[MAX_PLAYERS];
};

#endif // TCP_SERVER_H#pragma once
