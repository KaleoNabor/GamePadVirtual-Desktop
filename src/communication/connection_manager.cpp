#include "connection_manager.h"
#include <QDebug>
#include <QtBluetooth/QLowEnergyController>
#include "../protocol/gamepad_packet.h"

ConnectionManager::ConnectionManager(GamepadManager* gamepadManager, QObject* parent)
    : QObject(parent), m_gamepadManager(gamepadManager), m_isBleSupported(false)
{
    // Detecção de suporte a BLE Peripheral
    QLowEnergyController* testController = QLowEnergyController::createPeripheral();
    if (testController) {
        m_isBleSupported = true;
        delete testController;
        qInfo() << "Detectado: Suporte a Bluetooth Low Energy (BLE) em modo Periferico.";
        emit logMessage("Modo Bluetooth: LE (Recomendado)");
    }
    else {
        m_isBleSupported = false;
        qWarning() << "Aviso: Hardware nao suporta Bluetooth Low Energy (BLE) em modo Periferico. Usando Bluetooth Classico como alternativa.";
        emit logMessage("Modo Bluetooth: Classico (Compatibilidade)");
    }

    // Configuração do servidor de descoberta Wi-Fi
    m_discoveryServer = new WifiServer();
    m_discoveryServer->moveToThread(&m_discoveryThread);
    connect(&m_discoveryThread, &QThread::started, m_discoveryServer, [=]() { m_discoveryServer->startListening(27016); });

    // Configuração do servidor UDP para dados
    m_udpServer = new UdpServer();
    m_udpServer->moveToThread(&m_udpDataThread);
    connect(&m_udpDataThread, &QThread::started, m_udpServer, [=]() { m_udpServer->startServer(27015); });
    connect(m_udpServer, &UdpServer::packetReceived, m_gamepadManager, &GamepadManager::onPacketReceived);
    connect(m_udpServer, &UdpServer::playerConnected, m_gamepadManager, &GamepadManager::playerConnected);
    connect(m_udpServer, &UdpServer::playerDisconnected, m_gamepadManager, &GamepadManager::playerDisconnected);
    connect(m_udpServer, &UdpServer::logMessage, this, &ConnectionManager::logMessage);

    // Configuração do servidor Bluetooth clássico
    m_bluetoothServer = new BluetoothServer();
    m_bluetoothServer->moveToThread(&m_bluetoothThread);
    connect(&m_bluetoothThread, &QThread::started, m_bluetoothServer, &BluetoothServer::startServer);
    connect(m_bluetoothServer, &BluetoothServer::packetReceived, m_gamepadManager, &GamepadManager::onPacketReceived);
    connect(m_bluetoothServer, &BluetoothServer::playerConnected, m_gamepadManager, &GamepadManager::playerConnected);
    connect(m_bluetoothServer, &BluetoothServer::playerDisconnected, m_gamepadManager, &GamepadManager::playerDisconnected);
    connect(m_bluetoothServer, &BluetoothServer::logMessage, this, &ConnectionManager::logMessage);

    // Configuração do servidor BLE
    m_bleServer = new BleServer();
    m_bleServer->moveToThread(&m_bleThread);
    connect(&m_bleThread, &QThread::started, m_bleServer, &BleServer::startServer);
    connect(m_bleServer, &BleServer::packetReceived, this, [this](int playerIndex, const QByteArray& packetData) {
        if (packetData.size() >= static_cast<int>(sizeof(GamepadPacket))) {
            const GamepadPacket* packet = reinterpret_cast<const GamepadPacket*>(packetData.constData());
            m_gamepadManager->onPacketReceived(playerIndex, *packet);
        }
        });
    connect(m_bleServer, &BleServer::playerConnected, m_gamepadManager, &GamepadManager::playerConnected);
    connect(m_bleServer, &BleServer::playerDisconnected, m_gamepadManager, &GamepadManager::playerDisconnected);
    connect(m_bleServer, &BleServer::logMessage, this, &ConnectionManager::logMessage);

    // Conexão do sinal de vibração com lógica condicional
    connect(m_gamepadManager, &GamepadManager::vibrationCommandReady, this, [this](int playerIndex, const QByteArray& command) {
        // Tentativa de envio via UDP primeiro
        if (m_udpServer->sendToPlayer(playerIndex, command)) return;

        // Decisão entre BLE ou Bluetooth clássico
        if (m_isBleSupported) {
            m_bleServer->sendVibration(playerIndex, command);
        }
        else {
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
    // Início dos serviços obrigatórios
    if (!m_discoveryThread.isRunning()) m_discoveryThread.start();
    if (!m_udpDataThread.isRunning()) m_udpDataThread.start();

    // Início do serviço Bluetooth baseado na detecção
    if (m_isBleSupported) {
        if (!m_bleThread.isRunning()) m_bleThread.start();
    }
    else {
        if (!m_bluetoothThread.isRunning()) m_bluetoothThread.start();
    }
}

void ConnectionManager::stopServices()
{
    qDebug() << "Parando servicos de conexao...";
    // Parada dos serviços de descoberta e UDP
    if (m_discoveryThread.isRunning()) { m_discoveryThread.quit(); m_discoveryThread.wait(1000); }
    if (m_udpDataThread.isRunning()) { QMetaObject::invokeMethod(m_udpServer, "stopServer", Qt::BlockingQueuedConnection); m_udpDataThread.quit(); m_udpDataThread.wait(1000); }

    // Parada do serviço Bluetooth ativo
    if (m_bleThread.isRunning()) {
        QMetaObject::invokeMethod(m_bleServer, "stopServer", Qt::BlockingQueuedConnection);
        m_bleThread.quit();
        m_bleThread.wait(1000);
    }
    if (m_bluetoothThread.isRunning()) {
        QMetaObject::invokeMethod(m_bluetoothServer, "stopServer", Qt::BlockingQueuedConnection);
        m_bluetoothThread.quit();
        m_bluetoothThread.wait(1000);
    }
}