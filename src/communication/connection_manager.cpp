#include "connection_manager.h"
#include <QDebug>

ConnectionManager::ConnectionManager(GamepadManager* gamepadManager, QObject* parent)
    : QObject(parent), m_gamepadManager(gamepadManager)
{
    // --- Configura��o do Servidor de Descoberta (UDP) ---
    m_discoveryServer = new WifiServer();
    m_discoveryServer->moveToThread(&m_discoveryThread);
    // Inicia o servidor de descoberta na porta 27016 (porta diferente para n�o haver conflito)
    connect(&m_discoveryThread, &QThread::started, m_discoveryServer, [=]() {
        m_discoveryServer->startListening(27016);
        });

    // --- Configura��o do Servidor de Dados (TCP) ---
    m_tcpServer = new TcpServer();
    m_tcpServer->moveToThread(&m_tcpDataThread);
    // Inicia o servidor de dados na porta 27015
    connect(&m_tcpDataThread, &QThread::started, m_tcpServer, [=]() {
        m_tcpServer->startServer(27015);
        });
    // Conecta os sinais do servidor TCP ao GamepadManager
    connect(m_tcpServer, &TcpServer::packetReceived, m_gamepadManager, &GamepadManager::onPacketReceived, Qt::QueuedConnection);
    connect(m_tcpServer, &TcpServer::playerConnected, m_gamepadManager, &GamepadManager::playerConnected, Qt::QueuedConnection);
    connect(m_tcpServer, &TcpServer::playerDisconnected, m_gamepadManager, &GamepadManager::playerDisconnected, Qt::QueuedConnection);


    // --- Configura��o do Servidor Bluetooth (sem altera��es) ---
    m_bluetoothServer = new BluetoothServer();
    m_bluetoothServer->moveToThread(&m_bluetoothThread);
    connect(&m_bluetoothThread, &QThread::started, m_bluetoothServer, &BluetoothServer::startServer);
    connect(m_bluetoothServer, &BluetoothServer::packetReceived, m_gamepadManager, &GamepadManager::onPacketReceived, Qt::QueuedConnection);
    connect(m_bluetoothServer, &BluetoothServer::playerConnected, m_gamepadManager, &GamepadManager::playerConnected, Qt::QueuedConnection);
    connect(m_bluetoothServer, &BluetoothServer::playerDisconnected, m_gamepadManager, &GamepadManager::playerDisconnected, Qt::QueuedConnection);
    connect(m_bluetoothServer, &BluetoothServer::logMessage, this, &ConnectionManager::logMessage);
}

ConnectionManager::~ConnectionManager()
{
    stopServices();
}

void ConnectionManager::startServices()
{
    qDebug() << "Iniciando servicos de conexao...";
    if (!m_discoveryThread.isRunning()) m_discoveryThread.start();
    if (!m_tcpDataThread.isRunning()) m_tcpDataThread.start();
    if (!m_bluetoothThread.isRunning()) m_bluetoothThread.start();
}

void ConnectionManager::stopServices()
{
    qDebug() << "Parando servicos de conexao...";
    if (m_discoveryThread.isRunning()) {
        m_discoveryThread.quit();
        m_discoveryThread.wait(1000);
    }
    if (m_tcpDataThread.isRunning()) {
        QMetaObject::invokeMethod(m_tcpServer, "stopServer", Qt::BlockingQueuedConnection);
        m_tcpDataThread.quit();
        m_tcpDataThread.wait(1000);
    }
    if (m_bluetoothThread.isRunning()) {
        QMetaObject::invokeMethod(m_bluetoothServer, "stopServer", Qt::BlockingQueuedConnection);
        m_bluetoothThread.quit();
        m_bluetoothThread.wait(1000);
    }
}