#include "connection_manager.h"
#include <QDebug>
#include "../protocol/gamepad_packet.h" // Inclua o gamepad_packet.h

ConnectionManager::ConnectionManager(GamepadManager* gamepadManager, QObject* parent)
    : QObject(parent), m_gamepadManager(gamepadManager)
{
    // --- Configuração do Servidor de Descoberta (UDP) ---
    m_discoveryServer = new WifiServer();
    m_discoveryServer->moveToThread(&m_discoveryThread);
    connect(&m_discoveryThread, &QThread::started, m_discoveryServer, [=]() {
        m_discoveryServer->startListening(27016);
        });

    // --- Configuração do Servidor de Dados (UDP) ---
    m_udpServer = new UdpServer();
    m_udpServer->moveToThread(&m_udpDataThread);
    connect(&m_udpDataThread, &QThread::started, m_udpServer, [=]() {
        m_udpServer->startServer(27015);
        });
    connect(m_udpServer, &UdpServer::packetReceived, m_gamepadManager, &GamepadManager::onPacketReceived);
    connect(m_udpServer, &UdpServer::playerConnected, m_gamepadManager, &GamepadManager::playerConnected);
    connect(m_udpServer, &UdpServer::playerDisconnected, m_gamepadManager, &GamepadManager::playerDisconnected);
    connect(m_udpServer, &UdpServer::logMessage, this, &ConnectionManager::logMessage);

    // --- Configuração do Servidor Bluetooth Clássico ---
    m_bluetoothServer = new BluetoothServer();
    m_bluetoothServer->moveToThread(&m_bluetoothThread);
    connect(&m_bluetoothThread, &QThread::started, m_bluetoothServer, &BluetoothServer::startServer);
    connect(m_bluetoothServer, &BluetoothServer::packetReceived, m_gamepadManager, &GamepadManager::onPacketReceived);
    connect(m_bluetoothServer, &BluetoothServer::playerConnected, m_gamepadManager, &GamepadManager::playerConnected);
    connect(m_bluetoothServer, &BluetoothServer::playerDisconnected, m_gamepadManager, &GamepadManager::playerDisconnected);
    connect(m_bluetoothServer, &BluetoothServer::logMessage, this, &ConnectionManager::logMessage);

    // =========================================================================
    // NOVA CONFIGURAÇÃO DO SERVIDOR BLUETOOTH LOW ENERGY (BLE)
    // =========================================================================
    m_bleServer = new BleServer();
    m_bleServer->moveToThread(&m_bleThread);
    connect(&m_bleThread, &QThread::started, m_bleServer, &BleServer::startServer);
    // O sinal packetReceived do BleServer emite um QByteArray. Precisamos convertê-lo.
    connect(m_bleServer, &BleServer::packetReceived, this, [this](int playerIndex, const QByteArray& packetData) {
        if (packetData.size() >= static_cast<int>(sizeof(GamepadPacket))) {
            const GamepadPacket* packet = reinterpret_cast<const GamepadPacket*>(packetData.constData());
            m_gamepadManager->onPacketReceived(playerIndex, *packet);
        }
        });
    connect(m_bleServer, &BleServer::playerConnected, m_gamepadManager, &GamepadManager::playerConnected);
    connect(m_bleServer, &BleServer::playerDisconnected, m_gamepadManager, &GamepadManager::playerDisconnected);
    connect(m_bleServer, &BleServer::logMessage, this, &ConnectionManager::logMessage);


    // Conecta o sinal de vibração para ser enviado via TODOS os métodos
    connect(m_gamepadManager, &GamepadManager::vibrationCommandReady, this, [this](int playerIndex, const QByteArray& command) {
        if (!m_udpServer->sendToPlayer(playerIndex, command)) {
            if (!m_bleServer->sendVibration(playerIndex, command)) { // <<< TENTA ENVIAR VIA BLE
                m_bluetoothServer->sendToPlayer(playerIndex, command);
            }
        }
        });
}

ConnectionManager::~ConnectionManager()
{
    stopServices();
}

void ConnectionManager::startServices()
{
    qDebug() << "Iniciando servicos de conexao...";
    if (!m_discoveryThread.isRunning()) m_discoveryThread.start();
    if (!m_udpDataThread.isRunning()) m_udpDataThread.start();
    if (!m_bluetoothThread.isRunning()) m_bluetoothThread.start();
    if (!m_bleThread.isRunning()) m_bleThread.start(); // <<< INICIA A THREAD DO BLE
}

void ConnectionManager::stopServices()
{
    qDebug() << "Parando servicos de conexao...";
    if (m_discoveryThread.isRunning()) {
        m_discoveryThread.quit();
        m_discoveryThread.wait(1000);
    }
    if (m_udpDataThread.isRunning()) {
        QMetaObject::invokeMethod(m_udpServer, "stopServer", Qt::BlockingQueuedConnection);
        m_udpDataThread.quit();
        m_udpDataThread.wait(1000);
    }
    if (m_bluetoothThread.isRunning()) {
        QMetaObject::invokeMethod(m_bluetoothServer, "stopServer", Qt::BlockingQueuedConnection);
        m_bluetoothThread.quit();
        m_bluetoothThread.wait(1000);
    }
    // <<< PARA A THREAD DO BLE DE FORMA SEGURA >>>
    if (m_bleThread.isRunning()) {
        QMetaObject::invokeMethod(m_bleServer, "stopServer", Qt::BlockingQueuedConnection);
        m_bleThread.quit();
        m_bleThread.wait(1000);
    }
}