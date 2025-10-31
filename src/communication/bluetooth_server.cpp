#include "bluetooth_server.h"
#include <QBluetoothUuid>
#include <QDebug>
#include <QBluetoothLocalDevice>

// UUID do serviço Bluetooth clássico (Serial Port Profile)
static const QBluetoothUuid ServiceUuid(QStringLiteral("00001101-0000-1000-8000-00805F9B34FB"));

// --- CONSTRUTOR ---
// Inicializa o servidor Bluetooth e configura os slots de jogador
BluetoothServer::BluetoothServer(QObject* parent) : QObject(parent), m_btServer(nullptr)
{
    // Inicialização dos slots de jogador como livres (false = disponível)
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_playerSlots[i] = false;
    }
}

// --- DESTRUTOR ---
// Garante a limpeza adequada dos recursos do servidor
BluetoothServer::~BluetoothServer()
{
    stopServer();
}

// --- INICIAR SERVIDOR ---
// Configura e inicia o servidor Bluetooth
void BluetoothServer::startServer()
{
    // Verificação se servidor já está ativo (evita dupla inicialização)
    if (m_btServer) {
        return;
    }

    // --- SEÇÃO: VERIFICAÇÃO DE HARDWARE ---
    // Verificação da disponibilidade do adaptador Bluetooth
    QBluetoothLocalDevice localDevice;
    if (!localDevice.isValid() || localDevice.hostMode() == QBluetoothLocalDevice::HostPoweredOff) {
        qCritical() << "ERRO DE DIAGNÓSTICO: Adaptador Bluetooth não encontrado ou está desligado.";
        emit logMessage("Erro: O Bluetooth do PC está desligado ou indisponível.");
        return;
    }
    qDebug() << "Diagnóstico: Adaptador Bluetooth encontrado e ligado.";

    // --- SEÇÃO: CRIAÇÃO DO SERVIDOR ---
    // Criação do servidor Bluetooth RFCOMM (protocolo serial)
    m_btServer = new QBluetoothServer(QBluetoothServiceInfo::RfcommProtocol, this);
    connect(m_btServer, &QBluetoothServer::newConnection, this, &BluetoothServer::clientConnected);

    // Conexão do sinal de erro do servidor para diagnóstico
    connect(m_btServer, &QBluetoothServer::errorOccurred, this, [](QBluetoothServer::Error error) {
        qCritical() << "ERRO DE SERVIDOR BLUETOOTH:" << error;
        });

    // --- SEÇÃO: INICIALIZAÇÃO DA ESCUTA ---
    // Início da escuta em qualquer endereço local (endereço vazio = todos os adaptadores)
    const QBluetoothAddress address;
    if (!m_btServer->listen(address)) {
        qCritical() << "ERRO CRÍTICO: m_btServer->listen() falhou.";
        emit logMessage("Erro: Falha ao iniciar a escuta do servidor Bluetooth.");
        delete m_btServer;
        m_btServer = nullptr;
        return;
    }

    // Verificação se servidor está ouvindo corretamente
    if (m_btServer->isListening()) {
        qDebug() << "Diagnóstico: Servidor BT está ouvindo na porta:" << m_btServer->serverPort();
    }

    // --- SEÇÃO: REGISTRO DO SERVIÇO ---
    // Registro do serviço Bluetooth para descoberta pelos clientes
    QBluetoothServiceInfo serviceInfo;
    serviceInfo.setServiceUuid(ServiceUuid);
    serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceName, "GamePadVirtual Server");
    serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceDescription, "Servidor para o GamePadVirtual App");

    if (serviceInfo.registerService(address)) {
        qDebug() << "Diagnóstico: Serviço 'GamePadVirtual Server' registrado com sucesso.";
    }
    else {
        qCritical() << "ERRO CRÍTICO: Falha ao registrar o serviço Bluetooth.";
    }

    emit logMessage("Servidor Bluetooth aguardando conexões...");
}

// --- PARAR SERVIDOR ---
// Para o servidor e realiza limpeza completa de recursos
void BluetoothServer::stopServer()
{
    // Parada e limpeza do servidor Bluetooth
    if (m_btServer) {
        m_btServer->close();
        // Remove todos os sockets de clientes
        qDeleteAll(m_clientSockets);
        m_clientSockets.clear();
        m_socketPlayerMap.clear();
        delete m_btServer;
        m_btServer = nullptr;
    }
    qDebug() << "Servidor Bluetooth parado.";
}

// --- CLIENTE CONECTADO ---
// Trata novas conexões de clientes Bluetooth
void BluetoothServer::clientConnected()
{
    // Aceitação de nova conexão de cliente
    QBluetoothSocket* socket = m_btServer->nextPendingConnection();
    if (!socket) {
        return;
    }

    // --- SEÇÃO: VERIFICAÇÃO DE SLOTS DISPONÍVEIS ---
    int playerIndex = findEmptySlot();
    if (playerIndex == -1) {
        // Rejeição de conexão quando servidor está cheio
        qDebug() << "Servidor cheio. Rejeitando cliente Bluetooth:" << socket->peerName();
        QByteArray fullMessage = "{\"type\":\"system\",\"code\":\"server_full\"}";

        // Notifica o cliente e fecha a conexão
        socket->write(fullMessage);
        socket->waitForBytesWritten(100);
        socket->close();
        delete socket;
        return;
    }

    // --- SEÇÃO: CONFIGURAÇÃO DO SOCKET ---
    // Conexão dos sinais do socket para cliente aceito
    connect(socket, &QBluetoothSocket::readyRead, this, &BluetoothServer::readSocket);
    connect(socket, &QBluetoothSocket::disconnected, this, &BluetoothServer::clientDisconnected);

    // --- SEÇÃO: REGISTRO DO CLIENTE ---
    // Adição do socket às listas de controle
    m_clientSockets.append(socket);
    m_playerSlots[playerIndex] = true;
    m_socketPlayerMap[socket] = playerIndex;

    qDebug() << "Novo jogador" << (playerIndex + 1) << "conectado via Bluetooth:" << socket->peerName();
    emit playerConnected(playerIndex, "Bluetooth");
}

// --- LER SOCKET ---
// Processa dados recebidos dos clientes conectados
void BluetoothServer::readSocket()
{
    // Identifica qual socket está enviando dados
    QBluetoothSocket* socket = qobject_cast<QBluetoothSocket*>(sender());
    if (!socket || !m_socketPlayerMap.contains(socket)) return;

    int playerIndex = m_socketPlayerMap[socket];

    // Processamento de pacotes completos do gamepad
    while (socket->bytesAvailable() >= static_cast<qint64>(sizeof(GamepadPacket)))
    {
        QByteArray data = socket->read(sizeof(GamepadPacket));
        const GamepadPacket* packet = reinterpret_cast<const GamepadPacket*>(data.constData());
        emit packetReceived(playerIndex, *packet);
    }
}

// --- CLIENTE DESCONECTADO ---
// Trata a desconexão de clientes (natural ou forçada)
void BluetoothServer::clientDisconnected()
{
    // Identifica qual socket foi desconectado
    QBluetoothSocket* socket = qobject_cast<QBluetoothSocket*>(sender());
    if (!socket) return;

    // --- SEÇÃO: LIMPEZA DE RECURSOS ---
    if (m_socketPlayerMap.contains(socket)) {
        int playerIndex = m_socketPlayerMap[socket];
        m_playerSlots[playerIndex] = false;
        m_socketPlayerMap.remove(socket);
        m_clientSockets.removeAll(socket);
        socket->deleteLater(); // Marca o socket para exclusão segura

        qDebug() << "Jogador" << (playerIndex + 1) << "desconectado (Bluetooth).";
        emit playerDisconnected(playerIndex);
    }
}

// --- ENCONTRAR SLOT VAZIO ---
// Busca por slot de jogador disponível
int BluetoothServer::findEmptySlot() const
{
    // Percorre todos os slots procurando um disponível
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!m_playerSlots[i]) {
            return i; // Retorna o primeiro slot livre encontrado
        }
    }
    return -1; // Nenhum slot disponível
}

// --- ENVIAR PARA JOGADOR ---
// Envio de dados para jogador específico via Bluetooth
bool BluetoothServer::sendToPlayer(int playerIndex, const QByteArray& data)
{
    // Procura o socket correspondente ao índice do jogador
    for (auto it = m_socketPlayerMap.constBegin(); it != m_socketPlayerMap.constEnd(); ++it) {
        if (it.value() == playerIndex) {
            it.key()->write(data); // Envia dados pelo socket
            return true; // Sucesso no envio
        }
    }
    return false; // Jogador não encontrado
}

// --- NOVA FUNÇÃO ADICIONADA (REQ 2 FIX) ---
// Força a desconexão de um jogador específico via Bluetooth
void BluetoothServer::forceDisconnectPlayer(int playerIndex)
{
    QBluetoothSocket* targetSocket = nullptr;

    // Encontra o socket correspondente ao índice do jogador
    for (auto it = m_socketPlayerMap.constBegin(); it != m_socketPlayerMap.constEnd(); ++it) {
        if (it.value() == playerIndex) {
            targetSocket = it.key();
            break;
        }
    }

    if (targetSocket) {
        // Chamar close() irá acionar o sinal clientDisconnected(),
        // que por sua vez chama playerDisconnected() e limpa o slot.
        // Isso garante que a desconexão seja tratada de forma consistente.
        targetSocket->close();
    }
}