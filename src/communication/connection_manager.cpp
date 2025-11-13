#include "connection_manager.h"
#include "network_server.h"
#include "bluetooth_server.h"
#include "ble_server.h"
#include <QDebug>

ConnectionManager::ConnectionManager(GamepadManager* gamepadManager, QObject* parent)
    : QObject(parent), m_gamepadManager(gamepadManager)
{
    // Inicialização dos servidores
    m_networkServer = new NetworkServer(this);
    m_bluetoothServer = new BluetoothServer(this);
    m_bleServer = new BleServer(this);

    // Conexões do servidor de rede
    connect(m_networkServer, &NetworkServer::playerConnected, this, &ConnectionManager::playerConnected);
    connect(m_networkServer, &NetworkServer::playerDisconnected, this, &ConnectionManager::playerDisconnected);
    connect(m_networkServer, &NetworkServer::packetReceived, m_gamepadManager, &GamepadManager::onPacketReceived);
    connect(m_networkServer, &NetworkServer::logMessage, this, &ConnectionManager::logMessage);

    // Conexões do servidor Bluetooth clássico
    connect(m_bluetoothServer, &BluetoothServer::playerConnected, this, &ConnectionManager::playerConnected);
    connect(m_bluetoothServer, &BluetoothServer::playerDisconnected, this, &ConnectionManager::playerDisconnected);
    connect(m_bluetoothServer, &BluetoothServer::packetReceived, m_gamepadManager, &GamepadManager::onPacketReceived);
    connect(m_bluetoothServer, &BluetoothServer::logMessage, this, &ConnectionManager::logMessage);

    // Conexões do servidor BLE
    connect(m_bleServer, &BleServer::playerConnected, this, &ConnectionManager::playerConnected);
    connect(m_bleServer, &BleServer::playerDisconnected, this, &ConnectionManager::playerDisconnected);
    connect(m_bleServer, &BleServer::logMessage, this, &ConnectionManager::logMessage);
    connect(m_bleServer, &BleServer::packetReceived, this, &ConnectionManager::onBlePacketReceived);

    // Sistema de vibração
    connect(m_gamepadManager, &GamepadManager::vibrationCommandReady, this, &ConnectionManager::onVibrationCommandReady);
}

ConnectionManager::~ConnectionManager()
{
    stopServices();
}

void ConnectionManager::startServices()
{
    m_networkServer->startServer();
    m_bluetoothServer->startServer();
    m_bleServer->startServer();
    emit logMessage("Todos os servidores foram iniciados.");
}

void ConnectionManager::stopServices()
{
    m_networkServer->stopServer();
    m_bluetoothServer->stopServer();
    m_bleServer->stopServer();
    emit logMessage("Todos os servidores foram parados.");
}

void ConnectionManager::forceDisconnectPlayer(int playerIndex, const QString& type)
{
    if (type == "Wi-Fi" || type == "Ancoragem USB") {
        m_networkServer->forceDisconnectPlayer(playerIndex);
    }
    else if (type == "Bluetooth") {
        m_bluetoothServer->forceDisconnectPlayer(playerIndex);
    }
    else if (type == "Bluetooth LE") {
        m_bleServer->forceDisconnectPlayer(playerIndex);
    }
}

void ConnectionManager::onVibrationCommandReady(int playerIndex, const QByteArray& command)
{
    m_networkServer->sendVibration(playerIndex, command);
    m_bluetoothServer->sendToPlayer(playerIndex, command);
    m_bleServer->sendVibration(playerIndex, command);
}

void ConnectionManager::onBlePacketReceived(int playerIndex, const QByteArray& packet)
{
    if (packet.size() == sizeof(GamepadPacket)) {
        const GamepadPacket* gamepadPacket = reinterpret_cast<const GamepadPacket*>(packet.constData());
        m_gamepadManager->onPacketReceived(playerIndex, *gamepadPacket);
    }
    else {
        qWarning() << "Recebido pacote BLE com tamanho incorreto:" << packet.size();
    }
}