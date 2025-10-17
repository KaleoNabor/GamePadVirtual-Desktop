#include "ble_server.h"

#include <QtBluetooth/QLowEnergyAdvertisingData>
#include <QtBluetooth/QLowEnergyAdvertisingParameters> 
#include <QtBluetooth/QLowEnergyCharacteristicData>
#include <QtBluetooth/QLowEnergyDescriptorData>        
#include <QtBluetooth/QLowEnergyServiceData>
#include <QtBluetooth/QBluetoothUuid>                   
#include <QDebug>

// UUIDs para serviço e características BLE
static const QBluetoothUuid SERVICE_UUID(QStringLiteral("00001812-0000-1000-8000-00805f9b34fb"));
static const QBluetoothUuid INPUT_CHAR_UUID(QStringLiteral("00002a4d-0000-1000-8000-00805f9b34fb"));
static const QBluetoothUuid VIBRATION_CHAR_UUID(QStringLiteral("1a2b3c4d-5e6f-7a8b-9c0d-1e2f3a4b5c6d"));

BleServer::BleServer(QObject* parent) : QObject(parent)
{
    // Inicialização dos slots de jogador como livres
    for (int i = 0; i < 4; ++i) m_playerSlots[i] = false;
}

BleServer::~BleServer()
{
    stopServer();
}

void BleServer::startServer()
{
    // Para e recria o servidor se já existir
    if (m_bleController) {
        stopServer();
    }

    m_bleController = QLowEnergyController::createPeripheral(this);

    setupService();

    // Configuração dos dados de advertising
    QLowEnergyAdvertisingData advertisingData;
    advertisingData.setDiscoverability(QLowEnergyAdvertisingData::DiscoverabilityGeneral);
    advertisingData.setIncludePowerLevel(true);
    advertisingData.setLocalName("GamePadVirtual-Server");
    advertisingData.setServices(QList<QBluetoothUuid>() << SERVICE_UUID);

    // Conexão dos sinais de cliente
    connect(m_bleController, &QLowEnergyController::connected, this, &BleServer::onClientConnected);
    connect(m_bleController, &QLowEnergyController::disconnected, this, &BleServer::onClientDisconnected);

    // Início do advertising
    m_bleController->startAdvertising(QLowEnergyAdvertisingParameters(), advertisingData, advertisingData);
    qDebug() << "Servidor BLE iniciado e anunciando...";
    emit logMessage("Servidor Bluetooth LE aguardando conexões...");
}

void BleServer::stopServer()
{
    // Parada e limpeza do servidor BLE
    if (m_bleController) {
        m_bleController->stopAdvertising();
        delete m_bleController;
        m_bleController = nullptr;
    }
    if (m_gamepadService) {
        delete m_gamepadService;
        m_gamepadService = nullptr;
    }
    qDebug() << "Servidor BLE parado.";
}

void BleServer::setupService()
{
    // Configuração do serviço BLE principal
    QLowEnergyServiceData serviceData;
    serviceData.setType(QLowEnergyServiceData::ServiceTypePrimary);
    serviceData.setUuid(SERVICE_UUID);

    // Característica de entrada para dados do gamepad
    QLowEnergyCharacteristicData inputCharData;
    inputCharData.setUuid(INPUT_CHAR_UUID);
    inputCharData.setProperties(QLowEnergyCharacteristic::Write);
    inputCharData.setValue(QByteArray(2, 0));

    // Característica de vibração para feedback
    QLowEnergyCharacteristicData vibrationCharData;
    vibrationCharData.setUuid(VIBRATION_CHAR_UUID);
    vibrationCharData.setProperties(QLowEnergyCharacteristic::Write | QLowEnergyCharacteristic::Notify);

    // Descritor de configuração do cliente para notificações
    QLowEnergyDescriptorData clientConfig;
    clientConfig.setUuid(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
    clientConfig.setValue(QByteArray(2, 0));
    vibrationCharData.addDescriptor(clientConfig);

    // Adição das características ao serviço
    serviceData.addCharacteristic(inputCharData);
    serviceData.addCharacteristic(vibrationCharData);

    m_gamepadService = m_bleController->addService(serviceData);

    if (!m_gamepadService) {
        qDebug() << "Falha ao criar serviço BLE";
        return;
    }

    // Busca da característica de vibração
    auto characteristics = m_gamepadService->characteristics();
    for (const auto& characteristic : characteristics) {
        if (characteristic.uuid() == VIBRATION_CHAR_UUID) {
            m_vibrationCharacteristic = characteristic;
            break;
        }
    }

    // Conexão do sinal de escrita de característica
    connect(m_gamepadService, &QLowEnergyService::characteristicWritten,
        this, &BleServer::onCharacteristicWritten);
}

void BleServer::onCharacteristicWritten(const QLowEnergyCharacteristic& characteristic, const QByteArray& newValue)
{
    // Processamento de escrita na característica de entrada
    if (characteristic.uuid() == INPUT_CHAR_UUID) {
        QBluetoothAddress clientAddress = m_bleController->remoteAddress();
        if (!m_clientPlayerMap.contains(clientAddress)) {
            // Mapeamento de novo cliente para slot de jogador
            int playerIndex = findEmptySlot();
            if (playerIndex != -1) {
                m_playerSlots[playerIndex] = true;
                m_clientPlayerMap[clientAddress] = playerIndex;
                emit playerConnected(playerIndex, "Bluetooth LE");
            }
            else {
                qDebug() << "Nenhum slot disponível para cliente BLE";
                return;
            }
        }

        int playerIndex = m_clientPlayerMap[clientAddress];
        emit packetReceived(playerIndex, newValue);
    }
}

void BleServer::onClientConnected()
{
    qDebug() << "Cliente BLE conectado:" << m_bleController->remoteName();
}

void BleServer::onClientDisconnected()
{
    qDebug() << "Cliente BLE desconectado.";
    QBluetoothAddress clientAddress = m_bleController->remoteAddress();
    if (m_clientPlayerMap.contains(clientAddress)) {
        int slot = m_clientPlayerMap.value(clientAddress);
        m_playerSlots[slot] = false;
        m_clientPlayerMap.remove(clientAddress);
        emit playerDisconnected(slot);
    }
}

bool BleServer::sendVibration(int playerIndex, const QByteArray& command)
{
    // Envio de comando de vibração para jogador específico
    if (!m_gamepadService || !m_vibrationCharacteristic.isValid()) {
        return false;
    }

    // Busca do endereço do cliente pelo índice do jogador
    QBluetoothAddress targetAddress;
    bool found = false;
    for (auto it = m_clientPlayerMap.constBegin(); it != m_clientPlayerMap.constEnd(); ++it) {
        if (it.value() == playerIndex) {
            targetAddress = it.key();
            found = true;
            break;
        }
    }

    // Envio da notificação de vibração
    if (found && m_bleController && m_bleController->remoteAddress() == targetAddress) {
        m_gamepadService->writeCharacteristic(m_vibrationCharacteristic, command, QLowEnergyService::WriteWithoutResponse);
        return true;
    }
    return false;
}

int BleServer::findEmptySlot() const {
    // Busca por slot de jogador disponível
    for (int i = 0; i < 4; ++i) {
        if (!m_playerSlots[i]) return i;
    }
    return -1;
}