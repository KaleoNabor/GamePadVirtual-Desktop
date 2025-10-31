#include "udp_server.h"
#include <QDebug>

const QByteArray DISCONNECT_MESSAGE = "DISCONNECT_GPV_PLAYER";

UdpServer::UdpServer(QObject* parent) : QObject(parent), m_udpSocket(nullptr)
{
    // Inicializa todos os slots de jogador como livres
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_playerSlots[i] = false;
    }

    // Configura timer para processamento em tempo real
    m_processingTimer = new QTimer(this);
    m_processingTimer->setInterval(2);
    connect(m_processingTimer, &QTimer::timeout, this, &UdpServer::processPendingDatagrams);
}

UdpServer::~UdpServer()
{
    stopServer();
}

void UdpServer::startServer(quint16 port)
{
    if (m_udpSocket) return;

    m_udpSocket = new QUdpSocket(this);

    // Inicia servidor UDP na porta especificada
    if (m_udpSocket->bind(QHostAddress::Any, port)) {
        qDebug() << "Servidor de Dados (UDP) iniciado na porta" << port << ".";
        m_processingTimer->start();
    }
    else {
        qDebug() << "Erro: Nao foi possivel iniciar o servidor UDP na porta" << port;
        emit logMessage("Erro: Nao foi possivel iniciar o servidor UDP.");
    }
}

void UdpServer::stopServer()
{
    // Para o servidor e libera recursos
    m_processingTimer->stop();
    if (m_udpSocket) {
        m_udpSocket->close();
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
}

void UdpServer::processPendingDatagrams()
{
    // Processa todos os datagramas recebidos
    while (m_udpSocket && m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        QHostAddress senderAddress;
        quint16 senderPort;
        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &senderAddress, &senderPort);

        ClientId clientId = { senderAddress, senderPort };

        // Trata mensagens de desconexão
        if (datagram == DISCONNECT_MESSAGE) {
            if (m_clientPlayerMap.contains(clientId)) {
                int playerIndex = m_clientPlayerMap[clientId];
                qDebug() << "Jogador" << playerIndex + 1 << "enviou sinal de desconexao.";
                handlePlayerDisconnect(playerIndex);
            }
            continue;
        }

        // Valida tamanho do pacote
        if (datagram.size() < static_cast<qint64>(sizeof(GamepadPacket))) continue;

        // Gerencia conexão de novos clientes
        int playerIndex = -1;
        if (!m_clientPlayerMap.contains(clientId)) {
            playerIndex = findEmptySlot();
            if (playerIndex != -1) {
                m_playerSlots[playerIndex] = true;
                m_clientPlayerMap[clientId] = playerIndex;
                m_playerClientMap[playerIndex] = clientId;
                qDebug() << "Novo jogador" << (playerIndex + 1) << "conectado via UDP:" << senderAddress.toString();
                emit playerConnected(playerIndex, "Wi-Fi (UDP)");
            }
            else {
                continue;
            }
        }
        else {
            playerIndex = m_clientPlayerMap[clientId];
        }

        // Emite pacote recebido para processamento
        const GamepadPacket* packet = reinterpret_cast<const GamepadPacket*>(datagram.constData());
        emit packetReceived(playerIndex, *packet);
    }
}

bool UdpServer::sendToPlayer(int playerIndex, const QByteArray& data)
{
    // Envia dados para jogador específico
    if (m_udpSocket && m_playerClientMap.contains(playerIndex)) {
        const ClientId& clientId = m_playerClientMap[playerIndex];
        m_udpSocket->writeDatagram(data, clientId.address, clientId.port);
        return true;
    }
    return false;
}

int UdpServer::findEmptySlot() const
{
    // Encontra primeiro slot disponível
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!m_playerSlots[i]) {
            return i;
        }
    }
    return -1;
}

void UdpServer::handlePlayerDisconnect(int playerIndex)
{
    // Processa desconexão completa do jogador
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS || !m_playerSlots[playerIndex]) {
        return;
    }

    // Remove mapeamentos do cliente
    if (m_playerClientMap.contains(playerIndex)) {
        ClientId clientId = m_playerClientMap.value(playerIndex);
        m_clientPlayerMap.remove(clientId);
    }

    m_playerClientMap.remove(playerIndex);
    m_playerSlots[playerIndex] = false;

    emit playerDisconnected(playerIndex);
}