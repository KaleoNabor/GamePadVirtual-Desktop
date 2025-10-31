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

// --- CONSTRUTOR ---
// Inicializa o servidor BLE e configura os slots de jogador
BleServer::BleServer(QObject* parent) : QObject(parent)
{
    // Inicialização dos slots de jogador para 8 jogadores (todos livres)
    for (int i = 0; i < 8; ++i) m_playerSlots[i] = false;
}

// --- DESTRUTOR ---
// Garante a limpeza adequada dos recursos do servidor BLE
BleServer::~BleServer()
{
    stopServer();
}

// --- INICIAR SERVIDOR ---
// Configura e inicia o servidor BLE com advertising
void BleServer::startServer()
{
    // Para e recria o servidor se já existir (reinicialização)
    if (m_bleController) {
        stopServer();
    }

    m_bleController = QLowEnergyController::createPeripheral(this);

    setupService();

    // --- SEÇÃO: CONFIGURAÇÃO DO ADVERTISING ---
    // Configuração dos dados de advertising para descoberta
    QLowEnergyAdvertisingData advertisingData;
    advertisingData.setDiscoverability(QLowEnergyAdvertisingData::DiscoverabilityGeneral);
    advertisingData.setIncludePowerLevel(true);
    advertisingData.setLocalName("GamePadVirtual-Server");
    advertisingData.setServices(QList<QBluetoothUuid>() << SERVICE_UUID);

    // --- SEÇÃO: CONEXÃO DE SINAIS ---
    // Conexão dos sinais de cliente para gerenciamento de conexões
    connect(m_bleController, &QLowEnergyController::connected, this, &BleServer::onClientConnected);
    connect(m_bleController, &QLowEnergyController::disconnected, this, &BleServer::onClientDisconnected);

    // --- SEÇÃO: INÍCIO DO ADVERTISING ---
    // Início do advertising para permitir que clientes encontrem o servidor
    m_bleController->startAdvertising(QLowEnergyAdvertisingParameters(), advertisingData, advertisingData);
    qDebug() << "Servidor BLE iniciado e anunciando...";
    emit logMessage("Servidor Bluetooth LE aguardando conexões...");
}

// --- PARAR SERVIDOR ---
// Para o servidor e realiza limpeza completa de recursos
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

// --- CONFIGURAR SERVIÇO ---
// Configura o serviço BLE com características e descritores
void BleServer::setupService()
{
    // Configuração do serviço BLE principal
    QLowEnergyServiceData serviceData;
    serviceData.setType(QLowEnergyServiceData::ServiceTypePrimary);
    serviceData.setUuid(SERVICE_UUID);

    // --- SEÇÃO: CARACTERÍSTICA DE ENTRADA ---
    // Característica de entrada para dados do gamepad (escrita pelo cliente)
    QLowEnergyCharacteristicData inputCharData;
    inputCharData.setUuid(INPUT_CHAR_UUID);
    inputCharData.setProperties(QLowEnergyCharacteristic::Write);
    inputCharData.setValue(QByteArray(2, 0));

    // --- SEÇÃO: CARACTERÍSTICA DE VIBRAÇÃO ---
    // Característica de vibração para feedback háptico (escrita + notificação)
    QLowEnergyCharacteristicData vibrationCharData;
    vibrationCharData.setUuid(VIBRATION_CHAR_UUID);
    vibrationCharData.setProperties(QLowEnergyCharacteristic::Write | QLowEnergyCharacteristic::Notify);

    // --- SEÇÃO: DESCRITOR DE CONFIGURAÇÃO ---
    // Descritor de configuração do cliente para habilitar notificações
    QLowEnergyDescriptorData clientConfig;
    clientConfig.setUuid(QBluetoothUuid::DescriptorType::ClientCharacteristicConfiguration);
    clientConfig.setValue(QByteArray(2, 0));
    vibrationCharData.addDescriptor(clientConfig);

    // --- SEÇÃO: ADIÇÃO DAS CARACTERÍSTICAS ---
    // Adição das características ao serviço principal
    serviceData.addCharacteristic(inputCharData);
    serviceData.addCharacteristic(vibrationCharData);

    m_gamepadService = m_bleController->addService(serviceData);

    if (!m_gamepadService) {
        qDebug() << "Falha ao criar serviço BLE";
        return;
    }

    // --- SEÇÃO: BUSCA DA CARACTERÍSTICA DE VIBRAÇÃO ---
    // Busca da característica de vibração para referência futura
    auto characteristics = m_gamepadService->characteristics();
    for (const auto& characteristic : characteristics) {
        if (characteristic.uuid() == VIBRATION_CHAR_UUID) {
            m_vibrationCharacteristic = characteristic;
            break;
        }
    }

    // --- SEÇÃO: CONEXÃO DO SINAL DE ESCRITA ---
    // Conexão do sinal de escrita de característica para processar dados
    connect(m_gamepadService, &QLowEnergyService::characteristicWritten,
        this, &BleServer::onCharacteristicWritten);
}

// --- ESCRITA DE CARACTERÍSTICA ---
// Processa dados escritos nas características pelos clientes
void BleServer::onCharacteristicWritten(const QLowEnergyCharacteristic& characteristic, const QByteArray& newValue)
{
    // Processamento de escrita na característica de entrada
    if (characteristic.uuid() == INPUT_CHAR_UUID) {
        QBluetoothAddress clientAddress = m_bleController->remoteAddress();
        if (!m_clientPlayerMap.contains(clientAddress)) {
            // --- SEÇÃO: MAPEAMENTO DE NOVO CLIENTE ---
            // Mapeamento de novo cliente para slot de jogador
            int playerIndex = findEmptySlot();
            if (playerIndex != -1) {
                // Conexão bem-sucedida para slot disponível
                m_playerSlots[playerIndex] = true;
                m_clientPlayerMap[clientAddress] = playerIndex;
                emit playerConnected(playerIndex, "Bluetooth LE");
            }
            else {
                // --- SEÇÃO: REJEIÇÃO DE CONEXÃO ---
                // Rejeição de conexão quando servidor está cheio
                qDebug() << "Servidor cheio. Rejeitando cliente BLE:" << clientAddress.toString();
                QByteArray fullMessage = "{\"type\":\"system\",\"code\":\"server_full\"}";

                if (m_gamepadService && m_vibrationCharacteristic.isValid()) {
                    m_gamepadService->writeCharacteristic(m_vibrationCharacteristic, fullMessage, QLowEnergyService::WriteWithoutResponse);
                }
                return;
            }
        }

        // --- SEÇÃO: EMISSÃO DE PACOTE RECEBIDO ---
        int playerIndex = m_clientPlayerMap[clientAddress];
        emit packetReceived(playerIndex, newValue);
    }
}

// --- CLIENTE CONECTADO ---
// Trata novas conexões de clientes BLE
void BleServer::onClientConnected()
{
    qDebug() << "Cliente BLE conectado:" << m_bleController->remoteName();
}

// --- CLIENTE DESCONECTADO ---
// Trata a desconexão de clientes BLE (natural ou forçada)
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

// --- ENVIAR VIBRAÇÃO ---
// Envia comando de vibração para jogador específico via BLE
bool BleServer::sendVibration(int playerIndex, const QByteArray& command)
{
    // Envio de comando de vibração para jogador específico
    if (!m_gamepadService || !m_vibrationCharacteristic.isValid()) {
        return false;
    }

    // --- SEÇÃO: BUSCA DO CLIENTE ---
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

    // --- SEÇÃO: ENVIO DE NOTIFICAÇÃO ---
    // Envio da notificação de vibração para o cliente correto
    if (found && m_bleController && m_bleController->remoteAddress() == targetAddress) {
        m_gamepadService->writeCharacteristic(m_vibrationCharacteristic, command, QLowEnergyService::WriteWithoutResponse);
        return true;
    }
    return false;
}

// --- ENCONTRAR SLOT VAZIO ---
// Busca por slot de jogador disponível entre 8 slots
int BleServer::findEmptySlot() const {
    // Percorre todos os slots procurando um disponível
    for (int i = 0; i < 8; ++i) {
        if (!m_playerSlots[i]) return i; // Retorna o primeiro slot livre
    }
    return -1; // Nenhum slot disponível
}

// --- NOVA FUNÇÃO ADICIONADA (REQ 2 FIX) ---
// Força a desconexão de um jogador específico via BLE
void BleServer::forceDisconnectPlayer(int playerIndex)
{
    QBluetoothAddress targetAddress;
    bool found = false;

    // Encontra o endereço do cliente pelo índice do jogador
    for (auto it = m_clientPlayerMap.constBegin(); it != m_clientPlayerMap.constEnd(); ++it) {
        if (it.value() == playerIndex) {
            targetAddress = it.key();
            found = true;
            break;
        }
    }

    if (found) {
        // Se o cliente for o que está ativamente conectado ao controlador,
        // desconecta-o. Isso acionará 'onClientDisconnected'.
        if (m_bleController && m_bleController->remoteAddress() == targetAddress) {
            m_bleController->disconnectFromDevice();
        }
        // Se não for o cliente ativo (caso de borda), apenas limpa o slot
        else {
            m_playerSlots[playerIndex] = false;
            m_clientPlayerMap.remove(targetAddress);
            emit playerDisconnected(playerIndex);
        }
    }
}