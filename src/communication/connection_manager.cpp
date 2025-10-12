#include "connection_manager.h"
#include <QDebug>

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

    // Conecta os sinais do servidor UDP ao GamepadManager
    connect(m_udpServer, &UdpServer::packetReceived, m_gamepadManager, &GamepadManager::onPacketReceived, Qt::QueuedConnection);
    connect(m_udpServer, &UdpServer::playerConnected, m_gamepadManager, &GamepadManager::playerConnected, Qt::QueuedConnection);
    connect(m_udpServer, &UdpServer::playerDisconnected, m_gamepadManager, &GamepadManager::playerDisconnected, Qt::QueuedConnection);
    connect(m_udpServer, &UdpServer::logMessage, this, &ConnectionManager::logMessage);

    // --- Configuração do Servidor Bluetooth ---
    m_bluetoothServer = new BluetoothServer();
    m_bluetoothServer->moveToThread(&m_bluetoothThread);
    connect(&m_bluetoothThread, &QThread::started, m_bluetoothServer, &BluetoothServer::startServer);
    connect(m_bluetoothServer, &BluetoothServer::packetReceived, m_gamepadManager, &GamepadManager::onPacketReceived, Qt::QueuedConnection);
    connect(m_bluetoothServer, &BluetoothServer::playerConnected, m_gamepadManager, &GamepadManager::playerConnected, Qt::QueuedConnection);
    connect(m_bluetoothServer, &BluetoothServer::playerDisconnected, m_gamepadManager, &GamepadManager::playerDisconnected, Qt::QueuedConnection);
    connect(m_bluetoothServer, &BluetoothServer::logMessage, this, &ConnectionManager::logMessage);

    // Conecta o sinal de vibração para ser enviado via UDP ou Bluetooth
    connect(m_gamepadManager, &GamepadManager::vibrationCommandReady, this, [this](int playerIndex, const QByteArray& command) {
        // Tenta enviar via UDP primeiro
        if (!m_udpServer->sendToPlayer(playerIndex, command)) {
            // Se não for um jogador UDP, tenta enviar via Bluetooth
            m_bluetoothServer->sendToPlayer(playerIndex, command);
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
    if (!m_udpDataThread.isRunning()) m_udpDataThread.start(); // <<< USA UDP THREAD
    if (!m_bluetoothThread.isRunning()) m_bluetoothThread.start();
}

void ConnectionManager::stopServices()
{
    qDebug() << "Parando servicos de conexao...";
    if (m_discoveryThread.isRunning()) {
        m_discoveryThread.quit();
        m_discoveryThread.wait(1000);
    }
    if (m_udpDataThread.isRunning()) { // <<< USA UDP THREAD
        QMetaObject::invokeMethod(m_udpServer, "stopServer", Qt::BlockingQueuedConnection); // <<< USA UDP SERVER
        m_udpDataThread.quit(); // <<< USA UDP THREAD
        m_udpDataThread.wait(1000); // <<< USA UDP THREAD
    }
    if (m_bluetoothThread.isRunning()) {
        QMetaObject::invokeMethod(m_bluetoothServer, "stopServer", Qt::BlockingQueuedConnection);
        m_bluetoothThread.quit();
        m_bluetoothThread.wait(1000);
    }
}