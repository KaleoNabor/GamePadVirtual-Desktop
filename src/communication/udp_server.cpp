#include "udp_server.h"
#include <QDebug>

// Mensagem padr�o para desconex�o de jogadores
const QByteArray DISCONNECT_MESSAGE = "DISCONNECT_GPV_PLAYER";

// --- CONSTRUTOR ---
// Inicializa o servidor UDP e configura os timers
UdpServer::UdpServer(QObject* parent) : QObject(parent), m_udpSocket(nullptr)
{
    // Inicializa todos os slots de jogador como livres (false)
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_playerSlots[i] = false;
    }

    // Configura timer para processamento em tempo real
    m_processingTimer = new QTimer(this);
    m_processingTimer->setInterval(2); // Intervalo de 2ms para processamento r�pido
    connect(m_processingTimer, &QTimer::timeout, this, &UdpServer::processPendingDatagrams);
}

// --- DESTRUTOR ---
// Limpa recursos e para o servidor
UdpServer::~UdpServer()
{
    stopServer();
}

// --- INICIAR SERVIDOR ---
// Inicia o servidor UDP na porta especificada
void UdpServer::startServer(quint16 port)
{
    if (m_udpSocket) return; // Servidor j� est� rodando

    m_udpSocket = new QUdpSocket(this);

    // Tenta fazer o bind na porta especificada
    if (m_udpSocket->bind(QHostAddress::Any, port)) {
        qDebug() << "Servidor de Dados (UDP) iniciado na porta" << port << ".";
        m_processingTimer->start(); // Inicia o processamento cont�nuo
    }
    else {
        qDebug() << "Erro: Nao foi possivel iniciar o servidor UDP na porta" << port;
        emit logMessage("Erro: Nao foi possivel iniciar o servidor UDP.");
    }
}

// --- PARAR SERVIDOR ---
// Para o servidor e libera todos os recursos
void UdpServer::stopServer()
{
    // Para o timer de processamento
    m_processingTimer->stop();

    // Fecha e libera o socket UDP
    if (m_udpSocket) {
        m_udpSocket->close();
        delete m_udpSocket;
        m_udpSocket = nullptr;
    }
}

// --- PROCESSAR DATAGRAMAS PENDENTES ---
// Processa todos os datagramas recebidos dos clientes
void UdpServer::processPendingDatagrams()
{
    // Processa todos os datagramas na fila
    while (m_udpSocket && m_udpSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_udpSocket->pendingDatagramSize());
        QHostAddress senderAddress;
        quint16 senderPort;

        // L� o datagrama e obt�m informa��es do remetente
        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &senderAddress, &senderPort);

        ClientId clientId = { senderAddress, senderPort };

        // --- SE��O: TRATAMENTO DE DESCONEX�O ---
        // Verifica se � uma mensagem de desconex�o
        if (datagram == DISCONNECT_MESSAGE) {
            if (m_clientPlayerMap.contains(clientId)) {
                int playerIndex = m_clientPlayerMap[clientId];
                qDebug() << "Jogador" << playerIndex + 1 << "enviou sinal de desconexao.";
                handlePlayerDisconnect(playerIndex); // Processa a desconex�o
            }
            continue; // Pula para o pr�ximo datagrama
        }

        // --- SE��O: VALIDA��O DO PACOTE ---
        // Verifica se o pacote tem tamanho m�nimo necess�rio
        if (datagram.size() < static_cast<qint64>(sizeof(GamepadPacket))) continue;

        // --- SE��O: GERENCIAMENTO DE CONEX�ES ---
        // Gerencia conex�o de novos clientes ou clientes existentes
        int playerIndex = -1;
        if (!m_clientPlayerMap.contains(clientId)) {
            playerIndex = findEmptySlot(); // Procura slot dispon�vel
            if (playerIndex != -1) {
                // --- CONEX�O BEM-SUCEDIDA ---
                // Ativa o slot do jogador
                m_playerSlots[playerIndex] = true;
                // Atualiza mapeamentos bidirecionais
                m_clientPlayerMap[clientId] = playerIndex;
                m_playerClientMap[playerIndex] = clientId;
                qDebug() << "Novo jogador" << (playerIndex + 1) << "conectado via UDP:" << senderAddress.toString();
                emit playerConnected(playerIndex, "Wi-Fi (UDP)"); // Notifica nova conex�o
            }
            else {
                // --- SERVIDOR CHEIO ---
                // Rejeita conex�o quando n�o h� slots dispon�veis
                qDebug() << "Servidor cheio. Rejeitando cliente UDP:" << senderAddress.toString();
                QByteArray fullMessage = "{\"type\":\"system\",\"code\":\"server_full\"}";

                // Envia mensagem de servidor cheio para o cliente
                m_udpSocket->writeDatagram(fullMessage, senderAddress, senderPort);
                continue; // Pula para o pr�ximo datagrama
            }
        }
        else {
            // Cliente j� existe, obt�m seu �ndice
            playerIndex = m_clientPlayerMap[clientId];
        }

        // --- SE��O: PROCESSAMENTO DO PACOTE ---
        // Converte e emite o pacote de gamepad recebido
        const GamepadPacket* packet = reinterpret_cast<const GamepadPacket*>(datagram.constData());
        emit packetReceived(playerIndex, *packet); // Encaminha para processamento
    }
}

// --- ENVIAR PARA JOGADOR ---
// Envia dados para um jogador espec�fico
bool UdpServer::sendToPlayer(int playerIndex, const QByteArray& data)
{
    // Verifica se o socket existe e o jogador est� conectado
    if (m_udpSocket && m_playerClientMap.contains(playerIndex)) {
        const ClientId& clientId = m_playerClientMap[playerIndex];
        // Envia dados para o endere�o e porta do jogador
        m_udpSocket->writeDatagram(data, clientId.address, clientId.port);
        return true; // Sucesso no envio
    }
    return false; // Falha no envio
}

// --- ENCONTRAR SLOT VAZIO ---
// Procura o primeiro slot dispon�vel para novo jogador
int UdpServer::findEmptySlot() const
{
    // Percorre todos os slots procurando um dispon�vel
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!m_playerSlots[i]) {
            return i; // Retorna o primeiro slot livre encontrado
        }
    }
    return -1; // Nenhum slot dispon�vel
}

// --- MANIPULAR DESCONEX�O DE JOGADOR ---
// Processa a desconex�o completa de um jogador
void UdpServer::handlePlayerDisconnect(int playerIndex)
{
    // Valida se o �ndice � v�lido e o slot est� ocupado
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS || !m_playerSlots[playerIndex]) {
        return; // �ndice inv�lido ou slot j� est� vazio
    }

    // --- SE��O: LIMPEZA DE MAPEAMENTOS ---
    // Remove mapeamentos do cliente dos hashes
    if (m_playerClientMap.contains(playerIndex)) {
        ClientId clientId = m_playerClientMap.value(playerIndex);
        m_clientPlayerMap.remove(clientId); // Remove do mapeamento cliente->jogador
    }

    m_playerClientMap.remove(playerIndex); // Remove do mapeamento jogador->cliente
    m_playerSlots[playerIndex] = false; // Libera o slot

    // Notifica outros componentes sobre a desconex�o
    emit playerDisconnected(playerIndex);
}

// --- NOVA FUN��O ADICIONADA (REQ 2 FIX) ---
// For�a a desconex�o de um jogador espec�fico
void UdpServer::forceDisconnectPlayer(int playerIndex)
{
    // Apenas chama a fun��o de limpeza interna
    // que j� usamos para a desconex�o normal.
    // Ela j� define m_playerSlots[playerIndex] = false;
    handlePlayerDisconnect(playerIndex);
}