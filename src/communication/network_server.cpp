#include "network_server.h"
#include <QNetworkDatagram>
#include <QHostInfo>

NetworkServer::NetworkServer(QObject* parent)
    : QObject(parent),
    m_tcpServer(nullptr),
    m_udpSocket(nullptr),
    m_discoverySocket(nullptr)
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_playerSlots[i] = false;
    }
}

NetworkServer::~NetworkServer()
{
    stopServer();
}

void NetworkServer::startServer()
{
    // Servidor de controle TCP
    m_tcpServer = new QTcpServer(this);
    connect(m_tcpServer, &QTcpServer::newConnection, this, &NetworkServer::newTcpConnection);

    if (!m_tcpServer->listen(QHostAddress::Any, CONTROL_PORT_TCP)) {
        qCritical() << "Falha ao iniciar servidor TCP na porta" << CONTROL_PORT_TCP;
        emit logMessage("Erro: Falha ao iniciar servidor TCP.");
        delete m_tcpServer;
        m_tcpServer = nullptr;
        return;
    }

    // Servidor de dados UDP
    m_udpSocket = new QUdpSocket(this);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetworkServer::readUdpDatagrams);

    if (!m_udpSocket->bind(QHostAddress::Any, DATA_PORT_UDP)) {
        qCritical() << "Falha ao iniciar servidor UDP na porta" << DATA_PORT_UDP;
        emit logMessage("Erro: Falha ao iniciar servidor UDP.");
        stopServer();
        return;
    }

    // Servidor de descoberta UDP
    m_discoverySocket = new QUdpSocket(this);
    connect(m_discoverySocket, &QUdpSocket::readyRead, this, &NetworkServer::readDiscoveryDatagrams);

    if (!m_discoverySocket->bind(QHostAddress::AnyIPv4, DISCOVERY_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {
        qCritical() << "Falha ao iniciar servidor de Descoberta na porta" << DISCOVERY_PORT;
        emit logMessage("Erro: Falha ao iniciar servidor de Descoberta.");
        stopServer();
        return;
    }

    emit logMessage(QString("Servidor de Rede iniciado (TCP: %1, UDP: %2, Descoberta: %3)")
        .arg(CONTROL_PORT_TCP)
        .arg(DATA_PORT_UDP)
        .arg(DISCOVERY_PORT));
}

void NetworkServer::stopServer()
{
    if (m_tcpServer) {
        m_tcpServer->close();
        delete m_tcpServer;
        m_tcpServer = nullptr;
    }
    if (m_udpSocket) {
        m_udpSocket->close();
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
    if (m_discoverySocket) {
        m_discoverySocket->close();
        delete m_discoverySocket;
        m_discoverySocket = nullptr;
    }

    qDeleteAll(m_socketPlayerMap.keys());
    m_socketPlayerMap.clear();
    m_ipPlayerMap.clear();
    m_playerIpMap.clear();
    m_playerUdpPortMap.clear();

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_playerSlots[i] = false;
    }
    qDebug() << "Servidor de Rede parado.";
}

int NetworkServer::findEmptySlot() const
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!m_playerSlots[i]) {
            return i;
        }
    }
    return -1;
}

// Canal de controle TCP
void NetworkServer::newTcpConnection()
{
    QTcpSocket* socket = m_tcpServer->nextPendingConnection();
    if (!socket) return;

    socket->setParent(this);

    QHostAddress clientAddress = socket->peerAddress();
    if (clientAddress.protocol() == QAbstractSocket::IPv6Protocol) {
        clientAddress = QHostAddress(clientAddress.toIPv4Address());
    }

    int playerIndex = findEmptySlot();
    if (playerIndex == -1 || m_ipPlayerMap.contains(clientAddress)) {
        qDebug() << "Rejeitando conexão TCP de" << clientAddress.toString();
        socket->close();
        socket->deleteLater();
        return;
    }

    // CORREÇÃO: Usando a sintaxe de slot de membro.
    // Isso corrige os erros 'sender' e 'm_socketPlayerMap' indefinidos
    connect(socket, &QTcpSocket::disconnected, this, &NetworkServer::tcpClientDisconnected);
    connect(socket, &QTcpSocket::readyRead, this, &NetworkServer::readTcpSocket);
    // CORREÇÃO: Conectando o sinal de erro
    connect(socket, &QTcpSocket::errorOccurred, this, &NetworkServer::tcpSocketError);

    m_playerSlots[playerIndex] = true;
    m_socketPlayerMap[socket] = playerIndex;
    m_ipPlayerMap[clientAddress] = playerIndex;
    m_playerIpMap[playerIndex] = clientAddress;

    qDebug() << "Novo jogador" << (playerIndex + 1) << "conectado via TCP/IP:" << clientAddress.toString();
    emit playerConnected(playerIndex, "Wi-Fi");
}

void NetworkServer::tcpClientDisconnected()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket || !m_socketPlayerMap.contains(socket)) return;

    int playerIndex = m_socketPlayerMap[socket];
    QHostAddress clientAddress = socket->peerAddress();
    if (clientAddress.protocol() == QAbstractSocket::IPv6Protocol) {
        clientAddress = QHostAddress(clientAddress.toIPv4Address());
    }

    m_playerSlots[playerIndex] = false;
    m_socketPlayerMap.remove(socket);
    m_ipPlayerMap.remove(clientAddress);
    m_playerIpMap.remove(playerIndex);
    m_playerUdpPortMap.remove(playerIndex);

    socket->deleteLater();

    qDebug() << "Jogador" << (playerIndex + 1) << "desconectado (TCP/IP).";
    emit playerDisconnected(playerIndex);
}

void NetworkServer::readTcpSocket()
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (socket) {
        socket->readAll();
    }
}

// CORREÇÃO: Implementação da função de erro
void NetworkServer::tcpSocketError(QAbstractSocket::SocketError socketError)
{
    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
    if (!socket) return;

    qWarning() << "Erro de Socket TCP (Jogador" << m_socketPlayerMap.value(socket, -1) << "):" << socket->errorString();
    // A desconexão será tratada pelo slot tcpClientDisconnected, que é chamado automaticamente.
}

// Canal de dados UDP
void NetworkServer::readUdpDatagrams()
{
    while (m_udpSocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();

        QHostAddress senderAddress = datagram.senderAddress();
        if (senderAddress.protocol() == QAbstractSocket::IPv6Protocol) {
            senderAddress = QHostAddress(senderAddress.toIPv4Address());
        }

        quint16 senderPort = datagram.senderPort();

        if (!m_ipPlayerMap.contains(senderAddress)) {
            continue;
        }

        int playerIndex = m_ipPlayerMap[senderAddress];

        if (!m_playerUdpPortMap.contains(playerIndex)) {
            m_playerUdpPortMap.insert(playerIndex, senderPort);
            qDebug() << "Jogador" << (playerIndex + 1) << "registrou porta UDP:" << senderPort;
        }

        QByteArray data = datagram.data();

        if (data.size() == sizeof(GamepadPacket)) {
            const GamepadPacket* packet = reinterpret_cast<const GamepadPacket*>(data.constData());
            emit packetReceived(playerIndex, *packet);
        }
    }
}

// Canal de descoberta UDP
void NetworkServer::readDiscoveryDatagrams()
{
    while (m_discoverySocket->hasPendingDatagrams()) {
        QNetworkDatagram datagram = m_discoverySocket->receiveDatagram();
        QByteArray data = datagram.data();

        if (data == "DISCOVER_GAMEPAD_VIRTUAL_SERVER") {

            QHostAddress senderAddress = datagram.senderAddress();
            quint16 senderPort = datagram.senderPort();

            QString hostName = QHostInfo::localHostName();
            if (hostName.isEmpty()) {
                hostName = "Servidor-PC";
            }

            QByteArray response = "GAMEPAD_VIRTUAL_SERVER_ACK:" + hostName.toUtf8();

            m_discoverySocket->writeDatagram(response, senderAddress, senderPort);

            qDebug() << "Respondeu ao pedido de descoberta de" << senderAddress.toString();
        }
    }
}

void NetworkServer::forceDisconnectPlayer(int playerIndex)
{
    QTcpSocket* targetSocket = nullptr;
    for (auto it = m_socketPlayerMap.constBegin(); it != m_socketPlayerMap.constEnd(); ++it) {
        if (it.value() == playerIndex) {
            targetSocket = it.key();
            break;
        }
    }
    if (targetSocket) {
        targetSocket->close();
    }
}

void NetworkServer::sendVibration(int playerIndex, const QByteArray& command)
{
    if (m_playerIpMap.contains(playerIndex) && m_playerUdpPortMap.contains(playerIndex)) {
        QHostAddress address = m_playerIpMap.value(playerIndex);
        quint16 port = m_playerUdpPortMap.value(playerIndex);
        m_udpSocket->writeDatagram(command, address, port);
    }
}