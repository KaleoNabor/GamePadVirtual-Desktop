#include "connection_manager.h"
#include <QDebug>
#include <QtBluetooth/QLowEnergyController> // <<< INCLUA para a verificação
#include "../protocol/gamepad_packet.h"

ConnectionManager::ConnectionManager(GamepadManager* gamepadManager, QObject* parent)
    : QObject(parent), m_gamepadManager(gamepadManager), m_isBleSupported(false)
{
    // =========================================================================
    // PASSO 1: DETECTAR O SUPORTE A BLE PERIPHERAL
    // =========================================================================
    QLowEnergyController* testController = QLowEnergyController::createPeripheral();
    if (testController) {
        m_isBleSupported = true;
        delete testController; // Apenas criamos para testar, então deletamos
        qInfo() << "Detectado: Suporte a Bluetooth Low Energy (BLE) em modo Periferico.";
        emit logMessage("Modo Bluetooth: LE (Recomendado)");
    }
    else {
        m_isBleSupported = false;
        qWarning() << "Aviso: Hardware nao suporta Bluetooth Low Energy (BLE) em modo Periferico. Usando Bluetooth Classico como alternativa.";
        emit logMessage("Modo Bluetooth: Classico (Compatibilidade)");
    }

    // --- Configuração dos Servidores (preparar todos) ---
    m_discoveryServer = new WifiServer();
    m_discoveryServer->moveToThread(&m_discoveryThread);
    connect(&m_discoveryThread, &QThread::started, m_discoveryServer, [=]() { m_discoveryServer->startListening(27016); });

    m_udpServer = new UdpServer();
    m_udpServer->moveToThread(&m_udpDataThread);
    connect(&m_udpDataThread, &QThread::started, m_udpServer, [=]() { m_udpServer->startServer(27015); });
    connect(m_udpServer, &UdpServer::packetReceived, m_gamepadManager, &GamepadManager::onPacketReceived);
    connect(m_udpServer, &UdpServer::playerConnected, m_gamepadManager, &GamepadManager::playerConnected);
    connect(m_udpServer, &UdpServer::playerDisconnected, m_gamepadManager, &GamepadManager::playerDisconnected);
    connect(m_udpServer, &UdpServer::logMessage, this, &ConnectionManager::logMessage);

    m_bluetoothServer = new BluetoothServer();
    m_bluetoothServer->moveToThread(&m_bluetoothThread);
    connect(&m_bluetoothThread, &QThread::started, m_bluetoothServer, &BluetoothServer::startServer);
    connect(m_bluetoothServer, &BluetoothServer::packetReceived, m_gamepadManager, &GamepadManager::onPacketReceived);
    connect(m_bluetoothServer, &BluetoothServer::playerConnected, m_gamepadManager, &GamepadManager::playerConnected);
    connect(m_bluetoothServer, &BluetoothServer::playerDisconnected, m_gamepadManager, &GamepadManager::playerDisconnected);
    connect(m_bluetoothServer, &BluetoothServer::logMessage, this, &ConnectionManager::logMessage);

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

    // Conexão da vibração com lógica condicional
    connect(m_gamepadManager, &GamepadManager::vibrationCommandReady, this, [this](int playerIndex, const QByteArray& command) {
        // Tenta enviar via UDP primeiro, que é o mais comum
        if (m_udpServer->sendToPlayer(playerIndex, command)) return;

        // Se não for jogador UDP, decide qual Bluetooth usar
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
    if (!m_discoveryThread.isRunning()) m_discoveryThread.start();
    if (!m_udpDataThread.isRunning()) m_udpDataThread.start();

    // =========================================================================
    // PASSO 2: INICIAR APENAS A THREAD DE BLUETOOTH CORRETA
    // =========================================================================
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
    // Parar Discovery e UDP (sempre rodam)
    if (m_discoveryThread.isRunning()) { m_discoveryThread.quit(); m_discoveryThread.wait(1000); }
    if (m_udpDataThread.isRunning()) { QMetaObject::invokeMethod(m_udpServer, "stopServer", Qt::BlockingQueuedConnection); m_udpDataThread.quit(); m_udpDataThread.wait(1000); }

    // =========================================================================
    // PASSO 3: PARAR APENAS A THREAD DE BLUETOOTH QUE FOI INICIADA
    // =========================================================================
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