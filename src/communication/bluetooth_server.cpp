#include "bluetooth_server.h"
#include <QBluetoothUuid>
#include <QDebug>

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

    // Vers�o corrigida
    m_btServer = new QBluetoothServer(QBluetoothServiceInfo::RfcommProtocol, this);
    connect(m_btServer, &QBluetoothServer::newConnection, this, &BluetoothServer::clientConnected);

    // --- CORRE��O DEFINITIVA PARA QT 6 ---
    const QBluetoothAddress address;
    if (!m_btServer->listen(address)) {
        qDebug() << "Erro: Nao foi possivel iniciar o servidor Bluetooth.";
        emit logMessage("Erro: Nao foi possivel iniciar o servidor Bluetooth. Verifique se o BT est� ligado.");
        return;
    }

    QBluetoothServiceInfo serviceInfo;
    serviceInfo.setServiceUuid(ServiceUuid);
    serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceName, "GamePadVirtual Server");
    serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceDescription, "Servidor para o GamePadVirtual App");
    serviceInfo.registerService(address);
    // --- FIM DA CORRE��O ---

    qDebug() << "Servidor Bluetooth iniciado e aguardando conexoes...";
    emit logMessage("Servidor Bluetooth aguardando conex�es...");
}

// O resto do arquivo bluetooth_server.cpp continua igual...
// (As fun��es stopServer, clientConnected, readSocket, etc. n�o precisam de altera��o)
void BluetoothServer::stopServer()
{
    if (m_btServer) {
        // A API de registro de servi�o n�o tem um m�todo unregister expl�cito documentado,
        // a destrui��o do servidor deve cuidar disso.
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