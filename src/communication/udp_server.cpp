#include "udp_server.h"
#include <QDebug>

// Mensagem padrão para desconexão de jogadores
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
    m_processingTimer->setInterval(2); // Intervalo de 2ms para processamento rápido
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
    if (m_udpSocket) return; // Servidor já está rodando

    m_udpSocket = new QUdpSocket(this);

    // Tenta fazer o bind na porta especificada
    if (m_udpSocket->bind(QHostAddress::Any, port)) {
        qDebug() << "Servidor de Dados (UDP) iniciado na porta" << port << ".";
        m_processingTimer->start(); // Inicia o processamento contínuo
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

        // Lê o datagrama e obtém informações do remetente
        m_udpSocket->readDatagram(datagram.data(), datagram.size(), &senderAddress, &senderPort);

        ClientId clientId = { senderAddress, senderPort };

        // --- SEÇÃO: TRATAMENTO DE DESCONEXÃO ---
        // Verifica se é uma mensagem de desconexão
        if (datagram == DISCONNECT_MESSAGE) {
            if (m_clientPlayerMap.contains(clientId)) {
                int playerIndex = m_clientPlayerMap[clientId];
                qDebug() << "Jogador" << playerIndex + 1 << "enviou sinal de desconexao.";
                handlePlayerDisconnect(playerIndex); // Processa a desconexão
            }
            continue; // Pula para o próximo datagrama
        }

        // --- SEÇÃO: VALIDAÇÃO DO PACOTE ---
        // Verifica se o pacote tem tamanho mínimo necessário
        if (datagram.size() < static_cast<qint64>(sizeof(GamepadPacket))) continue;

        // --- SEÇÃO: GERENCIAMENTO DE CONEXÕES ---
        // Gerencia conexão de novos clientes ou clientes existentes
        int playerIndex = -1;
        if (!m_clientPlayerMap.contains(clientId)) {
            playerIndex = findEmptySlot(); // Procura slot disponível
            if (playerIndex != -1) {
                // --- CONEXÃO BEM-SUCEDIDA ---
                // Ativa o slot do jogador
                m_playerSlots[playerIndex] = true;
                // Atualiza mapeamentos bidirecionais
                m_clientPlayerMap[clientId] = playerIndex;
                m_playerClientMap[playerIndex] = clientId;

                // --- DETECÇÃO AUTOMÁTICA DO TIPO DE CONEXÃO ---
                QString address = senderAddress.toString();
                QString connectionType;

                // Sub-redes comuns de Ancoragem USB
                if (address.startsWith("192.168.42.") ||
                    address.startsWith("192.168.43.") ||
                    address.startsWith("192.168.100.")) {
                    connectionType = "Ancoragem USB";
                }
                else {
                    connectionType = "Wi-Fi (UDP)";
                }

                qDebug() << "Novo jogador" << (playerIndex + 1) << "conectado via" << connectionType << ":" << address;
                emit playerConnected(playerIndex, connectionType);
                // --- FIM DA MODIFICAÇÃO ---
            }
            else {
                // --- SERVIDOR CHEIO ---
                // Rejeita conexão quando não há slots disponíveis
                qDebug() << "Servidor cheio. Rejeitando cliente UDP:" << senderAddress.toString();
                QByteArray fullMessage = "{\"type\":\"system\",\"code\":\"server_full\"}";

                // Envia mensagem de servidor cheio para o cliente
                m_udpSocket->writeDatagram(fullMessage, senderAddress, senderPort);
                continue; // Pula para o próximo datagrama
            }
        }
        else {
            // Cliente já existe, obtém seu índice
            playerIndex = m_clientPlayerMap[clientId];
        }

        // --- SEÇÃO: PROCESSAMENTO DO PACOTE ---
        // Converte e emite o pacote de gamepad recebido
        const GamepadPacket* packet = reinterpret_cast<const GamepadPacket*>(datagram.constData());
        emit packetReceived(playerIndex, *packet); // Encaminha para processamento
    }
}

// --- ENVIAR PARA JOGADOR ---
// Envia dados para um jogador específico
bool UdpServer::sendToPlayer(int playerIndex, const QByteArray& data)
{
    // Verifica se o socket existe e o jogador está conectado
    if (m_udpSocket && m_playerClientMap.contains(playerIndex)) {
        const ClientId& clientId = m_playerClientMap[playerIndex];
        // Envia dados para o endereço e porta do jogador
        m_udpSocket->writeDatagram(data, clientId.address, clientId.port);
        return true; // Sucesso no envio
    }
    return false; // Falha no envio
}

// --- ENCONTRAR SLOT VAZIO ---
// Procura o primeiro slot disponível para novo jogador
int UdpServer::findEmptySlot() const
{
    // Percorre todos os slots procurando um disponível
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!m_playerSlots[i]) {
            return i; // Retorna o primeiro slot livre encontrado
        }
    }
    return -1; // Nenhum slot disponível
}

// --- MANIPULAR DESCONEXÃO DE JOGADOR ---
// Processa a desconexão completa de um jogador
void UdpServer::handlePlayerDisconnect(int playerIndex)
{
    // Valida se o índice é válido e o slot está ocupado
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS || !m_playerSlots[playerIndex]) {
        return; // Índice inválido ou slot já está vazio
    }

    // --- SEÇÃO: LIMPEZA DE MAPEAMENTOS ---
    // Remove mapeamentos do cliente dos hashes
    if (m_playerClientMap.contains(playerIndex)) {
        ClientId clientId = m_playerClientMap.value(playerIndex);
        m_clientPlayerMap.remove(clientId); // Remove do mapeamento cliente->jogador
    }

    m_playerClientMap.remove(playerIndex); // Remove do mapeamento jogador->cliente
    m_playerSlots[playerIndex] = false; // Libera o slot

    // Notifica outros componentes sobre a desconexão
    emit playerDisconnected(playerIndex);
}

// --- NOVA FUNÇÃO ADICIONADA (REQ 2 FIX) ---
// Força a desconexão de um jogador específico
void UdpServer::forceDisconnectPlayer(int playerIndex)
{
    // Apenas chama a função de limpeza interna
    // que já usamos para a desconexão normal.
    // Ela já define m_playerSlots[playerIndex] = false;
    handlePlayerDisconnect(playerIndex);
}