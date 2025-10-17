#include "udp_server.h"
#include <QDebug>

// Mensagem de desconex�o do protocolo
const QByteArray DISCONNECT_MESSAGE = "DISCONNECT_GPV_PLAYER";

UdpServer::UdpServer(QObject* parent) : QObject(parent), m_udpSocket(nullptr)
{
    // Inicializa��o dos slots de jogador como livres
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_playerSlots[i] = false;
    }

    // Configura��o do timer para processamento em alta frequ�ncia
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
    // Verifica��o se servidor j� est� ativo
    if (m_udpSocket) return;

    m_udpSocket = new QUdpSocket(this);

    // Bind do socket UDP na porta especificada
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
    // Parada do timer e limpeza do socket
    m_processingTimer->stop();
    if (m_udpSocket) {
        m_udpSocket->close();
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
}

void UdpServer::processPendingDatagrams()
{
    // Processamento de datagramas pendentes
    while (m_udpSocket && m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        QHostAddress senderAddress;
        quint16 senderPort;
        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &senderAddress, &senderPort);

        ClientId clientId = { senderAddress, senderPort };

        // Verifica��o de mensagem de desconex�o
        if (datagram == DISCONNECT_MESSAGE) {
            if (m_clientPlayerMap.contains(clientId)) {
                int playerIndex = m_clientPlayerMap[clientId];
                qDebug() << "Jogador" << playerIndex + 1 << "enviou sinal de desconexao.";
                handlePlayerDisconnect(playerIndex);
            }
            continue;
        }

        // Verifica��o do tamanho m�nimo do pacote
        if (datagram.size() < static_cast<qint64>(sizeof(GamepadPacket))) continue;

        // Processamento de novo cliente ou cliente existente
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

        // Emiss�o do pacote recebido
        const GamepadPacket* packet = reinterpret_cast<const GamepadPacket*>(datagram.constData());
        emit packetReceived(playerIndex, *packet);
    }
}

bool UdpServer::sendToPlayer(int playerIndex, const QByteArray& data)
{
    // Envio de dados para jogador espec�fico via UDP
    if (m_udpSocket && m_playerClientMap.contains(playerIndex)) {
        const ClientId& clientId = m_playerClientMap[playerIndex];
        m_udpSocket->writeDatagram(data, clientId.address, clientId.port);
        return true;
    }
    return false;
}

int UdpServer::findEmptySlot() const
{
    // Busca por slot de jogador dispon�vel
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!m_playerSlots[i]) {
            return i;
        }
    }
    return -1;
}

void UdpServer::handlePlayerDisconnect(int playerIndex)
{
    // Processamento de desconex�o de jogador
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS || !m_playerSlots[playerIndex]) {
        return;
    }

    // Remo��o do mapeamento de cliente
    if (m_playerClientMap.contains(playerIndex)) {
        ClientId clientId = m_playerClientMap.value(playerIndex);
        m_clientPlayerMap.remove(clientId);
    }

    // Limpeza dos mapeamentos principais
    m_playerClientMap.remove(playerIndex);

    // Libera��o do slot do jogador
    m_playerSlots[playerIndex] = false;

    // Emiss�o do sinal de desconex�o
    emit playerDisconnected(playerIndex);
}