#include "bluetooth_server.h"
#include <QBluetoothUuid>
#include <QDebug>
#include <QBluetoothLocalDevice> // <<< ADICIONE ESTE INCLUDE

static const QBluetoothUuid ServiceUuid(QStringLiteral("00001101-0000-1000-8000-00805F9B34FB"));

BluetoothServer::BluetoothServer(QObject* parent) : QObject(parent), m_btServer(nullptr)
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_playerSlots[i] = false;
    }
}

BluetoothServer::~BluetoothServer()
{
    stopServer();
}

void BluetoothServer::startServer()
{
    if (m_btServer) {
        return;
    }

    // <<< DIAGNÓSTICO 1: O Bluetooth do PC está ligado? >>>
    QBluetoothLocalDevice localDevice;
    if (!localDevice.isValid() || localDevice.hostMode() == QBluetoothLocalDevice::HostPoweredOff) {
        qCritical() << "ERRO DE DIAGNÓSTICO: Adaptador Bluetooth não encontrado ou está desligado.";
        emit logMessage("Erro: O Bluetooth do PC está desligado ou indisponível.");
        return;
    }
    qDebug() << "Diagnóstico: Adaptador Bluetooth encontrado e ligado.";


    m_btServer = new QBluetoothServer(QBluetoothServiceInfo::RfcommProtocol, this);
    connect(m_btServer, &QBluetoothServer::newConnection, this, &BluetoothServer::clientConnected);

    // <<< DIAGNÓSTICO 2: Ocorre algum erro durante a operação? >>>
    connect(m_btServer, &QBluetoothServer::errorOccurred, this, [](QBluetoothServer::Error error) {
        qCritical() << "ERRO DE SERVIDOR BLUETOOTH:" << error;
        });


    const QBluetoothAddress address; // Ouve em qualquer endereço local
    if (!m_btServer->listen(address)) {
        qCritical() << "ERRO CRÍTICO: m_btServer->listen() falhou.";
        emit logMessage("Erro: Falha ao iniciar a escuta do servidor Bluetooth.");
        delete m_btServer;
        m_btServer = nullptr;
        return;
    }

    // <<< DIAGNÓSTICO 3: O servidor está realmente ouvindo? >>>
    if (m_btServer->isListening()) {
        qDebug() << "Diagnóstico: Servidor BT está ouvindo na porta:" << m_btServer->serverPort();
    }


    QBluetoothServiceInfo serviceInfo;
    serviceInfo.setServiceUuid(ServiceUuid);
    serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceName, "GamePadVirtual Server");
    serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceDescription, "Servidor para o GamePadVirtual App");

    // <<< DIAGNÓSTICO 4: O registro do serviço teve sucesso? >>>
    if (serviceInfo.registerService(address)) {
        qDebug() << "Diagnóstico: Serviço 'GamePadVirtual Server' registrado com sucesso.";
    }
    else {
        qCritical() << "ERRO CRÍTICO: Falha ao registrar o serviço Bluetooth.";
    }

    emit logMessage("Servidor Bluetooth aguardando conexões...");
}

void BluetoothServer::stopServer()
{
    if (m_btServer) {
        // A API de registro de serviço não tem um método unregister explícito documentado,
        // a destruição do servidor deve cuidar disso.
        m_btServer->close();
        qDeleteAll(m_clientSockets);
        m_clientSockets.clear();
        m_socketPlayerMap.clear();
        delete m_btServer;
        m_btServer = nullptr;
    }
    qDebug() << "Servidor Bluetooth parado.";
}

void BluetoothServer::clientConnected()
{
    QBluetoothSocket* socket = m_btServer->nextPendingConnection();
    if (!socket) {
        return;
    }

    int playerIndex = findEmptySlot();
    if (playerIndex == -1) {
        qDebug() << "Maximo de jogadores conectados via Bluetooth. Rejeitando conexao.";
        socket->close();
        delete socket;
        return;
    }

    connect(socket, &QBluetoothSocket::readyRead, this, &BluetoothServer::readSocket);
    connect(socket, &QBluetoothSocket::disconnected, this, &BluetoothServer::clientDisconnected);

    m_clientSockets.append(socket);
    m_playerSlots[playerIndex] = true;
    m_socketPlayerMap[socket] = playerIndex;

    qDebug() << "Novo jogador" << (playerIndex + 1) << "conectado via Bluetooth:" << socket->peerName();
    emit playerConnected(playerIndex, "Bluetooth");
}

void BluetoothServer::readSocket()
{
    QBluetoothSocket* socket = qobject_cast<QBluetoothSocket*>(sender());
    if (!socket || !m_socketPlayerMap.contains(socket)) return;

    int playerIndex = m_socketPlayerMap[socket];

    while (socket->bytesAvailable() >= static_cast<qint64>(sizeof(GamepadPacket)))
    {
        QByteArray data = socket->read(sizeof(GamepadPacket));
        const GamepadPacket* packet = reinterpret_cast<const GamepadPacket*>(data.constData());
        emit packetReceived(playerIndex, *packet);
    }
}

void BluetoothServer::clientDisconnected()
{
    QBluetoothSocket* socket = qobject_cast<QBluetoothSocket*>(sender());
    if (!socket) return;

    if (m_socketPlayerMap.contains(socket)) {
        int playerIndex = m_socketPlayerMap[socket];
        m_playerSlots[playerIndex] = false;
        m_socketPlayerMap.remove(socket);
        m_clientSockets.removeAll(socket);
        socket->deleteLater();

        qDebug() << "Jogador" << (playerIndex + 1) << "desconectado (Bluetooth).";
        emit playerDisconnected(playerIndex);
    }
}

int BluetoothServer::findEmptySlot() const
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!m_playerSlots[i]) {
            return i;
        }
    }
    return -1;
}

bool BluetoothServer::sendToPlayer(int playerIndex, const QByteArray& data)
{
    // Percorre o mapa de sockets para encontrar o jogador correto
    for (auto it = m_socketPlayerMap.constBegin(); it != m_socketPlayerMap.constEnd(); ++it) {
        if (it.value() == playerIndex) {
            // Se encontrou o jogador, escreve os dados no socket dele
            it.key()->write(data);
            return true; // Retorna sucesso
        }
    }
    // Retorna falha se o jogador não foi encontrado entre os clientes Bluetooth
    return false;
}