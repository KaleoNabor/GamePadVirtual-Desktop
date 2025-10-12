#include "udp_server.h"
#include <QDebug>

const QByteArray DISCONNECT_MESSAGE = "DISCONNECT_GPV_PLAYER";

UdpServer::UdpServer(QObject* parent) : QObject(parent), m_udpSocket(nullptr)
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_playerSlots[i] = false;
    }

    // Configura o nosso "game loop" de processamento
    m_processingTimer = new QTimer(this);
    m_processingTimer->setInterval(2); // <<< Processa a cada 2ms (500 Hz)
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

    // <<< REMOVEMOS A CONEXÃO COM O SINAL readyRead DAQUI

    if (m_udpSocket->bind(QHostAddress::Any, port)) {
        qDebug() << "Servidor de Dados (UDP) iniciado na porta" << port << ".";
        m_processingTimer->start(); // <<< INICIAMOS O NOSSO LOOP AQUI
    }
    else {
        qDebug() << "Erro: Nao foi possivel iniciar o servidor UDP na porta" << port;
        emit logMessage("Erro: Nao foi possivel iniciar o servidor UDP.");
    }
}

void UdpServer::stopServer()
{
    m_processingTimer->stop(); // <<< PARAMOS O NOSSO LOOP
    if (m_udpSocket) {
        m_udpSocket->close();
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
}

// A função agora é chamada pelo nosso timer, não mais por um sinal
void UdpServer::processPendingDatagrams()
{
    while (m_udpSocket && m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        QHostAddress senderAddress;
        quint16 senderPort;
        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &senderAddress, &senderPort);

        ClientId clientId = { senderAddress, senderPort };

        // =========================================================================
        // NOVA LÓGICA: Verificar se o pacote é uma mensagem de desconexão
        // =========================================================================
        if (datagram == DISCONNECT_MESSAGE) {
            if (m_clientPlayerMap.contains(clientId)) {
                int playerIndex = m_clientPlayerMap[clientId];
                qDebug() << "Jogador" << playerIndex + 1 << "enviou sinal de desconexao.";
                handlePlayerDisconnect(playerIndex);
            }
            continue; // Pula para o próximo datagrama
        }

        // Se não for uma mensagem de desconexão, continua com a lógica normal do gamepad
        if (datagram.size() < static_cast<qint64>(sizeof(GamepadPacket))) continue;

        // O resto da função original permanece aqui...
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

        const GamepadPacket* packet = reinterpret_cast<const GamepadPacket*>(datagram.constData());
        emit packetReceived(playerIndex, *packet);
    }
}

// As outras funções (sendToPlayer, findEmptySlot) permanecem as mesmas
bool UdpServer::sendToPlayer(int playerIndex, const QByteArray& data)
{
    if (m_udpSocket && m_playerClientMap.contains(playerIndex)) {
        const ClientId& clientId = m_playerClientMap[playerIndex];
        m_udpSocket->writeDatagram(data, clientId.address, clientId.port);
        return true;
    }
    return false;
}

int UdpServer::findEmptySlot() const
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!m_playerSlots[i]) {
            return i;
        }
    }
    return -1;
}

void UdpServer::handlePlayerDisconnect(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS || !m_playerSlots[playerIndex]) {
        return; // Retorna se o índice for inválido ou o slot já estiver livre
    }

    // Encontra o ClientId para poder remover do mapa reverso
    if (m_playerClientMap.contains(playerIndex)) {
        ClientId clientId = m_playerClientMap.value(playerIndex);
        m_clientPlayerMap.remove(clientId);
    }

    // Remove o jogador dos mapeamentos principais
    m_playerClientMap.remove(playerIndex);

    // Libera o slot do jogador
    m_playerSlots[playerIndex] = false;

    // Emite o sinal para que a MainWindow e o GamepadManager sejam notificados
    emit playerDisconnected(playerIndex);
}