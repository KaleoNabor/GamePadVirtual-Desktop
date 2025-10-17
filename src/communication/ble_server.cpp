#include "ble_server.h"

#include <QtBluetooth/QLowEnergyAdvertisingData>
#include <QtBluetooth/QLowEnergyAdvertisingParameters> 
#include <QtBluetooth/QLowEnergyCharacteristicData>
#include <QtBluetooth/QLowEnergyDescriptorData>        
#include <QtBluetooth/QLowEnergyServiceData>
#include <QtBluetooth/QBluetoothUuid>                   
#include <QDebug>

// UUIDs corrigidos para Qt 6
static const QBluetoothUuid SERVICE_UUID(QStringLiteral("00001812-0000-1000-8000-00805f9b34fb")); // HID Service
static const QBluetoothUuid INPUT_CHAR_UUID(QStringLiteral("00002a4d-0000-1000-8000-00805f9b34fb")); // Report Characteristic
static const QBluetoothUuid VIBRATION_CHAR_UUID(QStringLiteral("1a2b3c4d-5e6f-7a8b-9c0d-1e2f3a4b5c6d")); // Custom Vibration

BleServer::BleServer(QObject* parent) : QObject(parent)
{
    for (int i = 0; i < 4; ++i) m_playerSlots[i] = false;
}

BleServer::~BleServer()
{
    stopServer();
}

void BleServer::startServer()
{
    if (m_bleController) {
        stopServer();
    }

    m_bleController = QLowEnergyController::createPeripheral(this);

    setupService();

    QLowEnergyAdvertisingData advertisingData;
    advertisingData.setDiscoverability(QLowEnergyAdvertisingData::DiscoverabilityGeneral);
    advertisingData.setIncludePowerLevel(true);
    advertisingData.setLocalName("GamePadVirtual-Server");
    advertisingData.setServices(QList<QBluetoothUuid>() << SERVICE_UUID);

    connect(m_bleController, &QLowEnergyController::connected, this, &BleServer::onClientConnected);
    connect(m_bleController, &QLowEnergyController::disconnected, this, &BleServer::onClientDisconnected);

    m_bleController->startAdvertising(QLowEnergyAdvertisingParameters(), advertisingData, advertisingData);
    qDebug() << "Servidor BLE iniciado e anunciando...";
    emit logMessage("Servidor Bluetooth LE aguardando conexões...");
}

void BleServer::stopServer()
{
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
    QLowEnergyServiceData serviceData;
    serviceData.setType(QLowEnergyServiceData::ServiceTypePrimary);
    serviceData.setUuid(SERVICE_UUID);

    // Característica de Input (onde o celular escreve)
    QLowEnergyCharacteristicData inputCharData;
    inputCharData.setUuid(INPUT_CHAR_UUID);
    inputCharData.setProperties(QLowEnergyCharacteristic::Write);
    inputCharData.setValue(QByteArray(2, 0));

    // Característica de Vibração (onde o PC escreve e o celular é notificado)
    QLowEnergyCharacteristicData vibrationCharData;
    vibrationCharData.setUuid(VIBRATION_CHAR_UUID);
    vibrationCharData.setProperties(QLowEnergyCharacteristic::Write | QLowEnergyCharacteristic::Notify);

    // Descritor de configuração do cliente para notificações
    QLowEnergyDescriptorData clientConfig;
    clientConfig.setUuid(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
    clientConfig.setValue(QByteArray(2, 0));
    vibrationCharData.addDescriptor(clientConfig);

    serviceData.addCharacteristic(inputCharData);
    serviceData.addCharacteristic(vibrationCharData);

    m_gamepadService = m_bleController->addService(serviceData);

    if (!m_gamepadService) {
        qDebug() << "Falha ao criar serviço BLE";
        return;
    }

    // Encontrar a característica de vibração
    auto characteristics = m_gamepadService->characteristics();
    for (const auto& characteristic : characteristics) {
        if (characteristic.uuid() == VIBRATION_CHAR_UUID) {
            m_vibrationCharacteristic = characteristic;
            break;
        }
    }

    connect(m_gamepadService, &QLowEnergyService::characteristicWritten,
        this, &BleServer::onCharacteristicWritten);
}

void BleServer::onCharacteristicWritten(const QLowEnergyCharacteristic& characteristic, const QByteArray& newValue)
{
    if (characteristic.uuid() == INPUT_CHAR_UUID) {
        QBluetoothAddress clientAddress = m_bleController->remoteAddress();
        if (!m_clientPlayerMap.contains(clientAddress)) {
            // Primeira escrita - mapear para um slot
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

    // O mapeamento real acontece na primeira escrita da característica
    // para evitar ocupar slots com clientes que não enviam dados
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
    if (!m_gamepadService || !m_vibrationCharacteristic.isValid()) {
        return false;
    }

    // Encontra o endereço do cliente para o playerIndex
    QBluetoothAddress targetAddress;
    bool found = false;
    for (auto it = m_clientPlayerMap.constBegin(); it != m_clientPlayerMap.constEnd(); ++it) {
        if (it.value() == playerIndex) {
            targetAddress = it.key();
            found = true;
            break;
        }
    }

    // Se o cliente conectado for o alvo, envia a notificação
    if (found && m_bleController && m_bleController->remoteAddress() == targetAddress) {
        m_gamepadService->writeCharacteristic(m_vibrationCharacteristic, command, QLowEnergyService::WriteWithoutResponse);
        return true;
    }
    return false;
}

int BleServer::findEmptySlot() const {
    for (int i = 0; i < 4; ++i) {
        if (!m_playerSlots[i]) return i;
    }
    return -1;
}