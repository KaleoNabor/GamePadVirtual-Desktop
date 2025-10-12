#include "tcp_server.h"
#include <QDebug>

TcpServer::TcpServer(QObject* parent) : QObject(parent), m_tcpServer(nullptr)
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_playerSlots[i] = false;
    }
}

TcpServer::~TcpServer()
{
    stopServer();
}

void TcpServer::startServer(quint16 port)
{
    if (m_tcpServer) return;

    m_tcpServer = new QTcpServer(this);
    connect(m_tcpServer, &QTcpServer::newConnection, this, &TcpServer::clientConnected);

    if (!m_tcpServer->listen(QHostAddress::Any, port)) {
        qDebug() << "Erro: Nao foi possivel iniciar o servidor TCP na porta" << port;
        return;
    }
    qDebug() << "Servidor de Dados (TCP) iniciado na porta" << port << ". Aguardando conexoes...";
}

void TcpServer::stopServer()
{
    if (m_tcpServer) {
        m_tcpServer->close();
        qDeleteAll(m_clientSockets);
        m_clientSockets.clear();
        m_socketPlayerMap.clear();
        delete m_tcpServer;
        m_tcpServer = nullptr;
    }
}

void TcpServer::clientConnected()
{
    QTcpSocket* socket = m_tcpServer->nextPendingConnection();
    if (!socket) return;

    // =========================================================================
    // SOLUÇÃO: Desativa o Algoritmo de Nagle para baixa latência
    // =========================================================================
    socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    int playerIndex = findEmptySlot();
    if (playerIndex == -1) {
        qDebug() << "Maximo de jogadores conectados. Rejeitando conexao TCP de" << socket->peerAddress().toString();
        socket->close();
        socket->deleteLater();
        return;
    }

    connect(socket, &QTcpSocket::readyRead, this, &TcpServer::readSocket);
    connect(socket, &QTcpSocket::disconnected, this, &TcpServer::clientDisconnected);

    m_clientSockets.append(socket);
    m_playerSlots[playerIndex] = true;
    m_socketPlayerMap[socket] = playerIndex;

    qDebug() << "Novo jogador" << (playerIndex + 1) << "conectado via TCP:" << socket->peerAddress().toString();
    QString connectionType = socket->peerAddress().toString().startsWith("192.168.") ? "Wi-Fi" : "Ancoragem USB";
    emit playerConnected(playerIndex, connectionType);
}

void TcpServer::readSocket()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !m_socketPlayerMap.contains(socket)) return;

    int playerIndex = m_socketPlayerMap[socket];

    while (socket->bytesAvailable() >= static_cast<qint64>(sizeof(GamepadPacket)))
    {
        QByteArray data = socket->read(sizeof(GamepadPacket));
        const GamepadPacket* packet = reinterpret_cast<const GamepadPacket*>(data.constData());
        emit packetReceived(playerIndex, *packet);
    }
}

void TcpServer::clientDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    if (m_socketPlayerMap.contains(socket)) {
        int playerIndex = m_socketPlayerMap[socket];
        m_playerSlots[playerIndex] = false;
        m_socketPlayerMap.remove(socket);
        m_clientSockets.removeAll(socket);
        socket->deleteLater();
        qDebug() << "Jogador" << (playerIndex + 1) << "desconectado (TCP).";
        emit playerDisconnected(playerIndex);
    }
}

int TcpServer::findEmptySlot() const
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!m_playerSlots[i]) {
            return i;
        }
    }
    return -1;
}