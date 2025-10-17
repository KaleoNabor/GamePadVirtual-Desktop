#define NOMINMAX
#include "gamepad_manager.h"
#include <QDebug>
#include <cmath>
#include <algorithm>

// Callback de notificação para vibração do controle
void CALLBACK GamepadManager::x360NotificationCallback(
    VigemClient Client, VigemTarget Target, UCHAR LargeMotor,
    UCHAR SmallMotor, UCHAR LedNumber, LPVOID UserData)
{
    Q_UNUSED(Client); Q_UNUSED(LedNumber);
    GamepadManager* manager = static_cast<GamepadManager*>(UserData);
    if (!manager) return;

    // Encontra o jogador correspondente ao target e processa vibração
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (manager->m_targets[i] == Target) {
            manager->handleVibration(i, LargeMotor, SmallMotor);
            break;
        }
    }
}

GamepadManager::GamepadManager(QObject* parent)
    : QObject(parent), m_client(nullptr)
{
    // Inicialização dos arrays de estado
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_targets[i] = nullptr;
        m_connected[i] = false;
        m_dirtyFlags[i].storeRelease(0);
    }

    // Configuração do timer para processamento em loop de jogo
    m_processingTimer = new QTimer(this);
    m_processingTimer->setInterval(8);
    connect(m_processingTimer, &QTimer::timeout, this, &GamepadManager::processLatestPackets);
    m_processingTimer->start();
}

GamepadManager::~GamepadManager()
{
    shutdown();
}

bool GamepadManager::initialize()
{
    // Alocação e conexão do cliente ViGEm
    m_client = vigem_alloc();
    if (m_client == nullptr) {
        qCritical() << "Falha ao alocar cliente ViGEm";
        return false;
    }
    const VIGEM_ERROR retval = vigem_connect(m_client);
    if (!VIGEM_SUCCESS(retval)) {
        qCritical() << "Falha ao conectar com ViGEm:" << retval;
        vigem_free(m_client);
        m_client = nullptr;
        return false;
    }
    qDebug() << "ViGEm inicializado com sucesso";
    return true;
}

void GamepadManager::shutdown()
{
    // Limpeza de todos os gamepads virtuais
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        cleanupGamepad(i);
    }
    if (m_client) {
        vigem_disconnect(m_client);
        vigem_free(m_client);
        m_client = nullptr;
    }
}

// Recebimento rápido de pacotes da thread de rede
void GamepadManager::onPacketReceived(int playerIndex, const GamepadPacket& packet)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    m_latestPackets[playerIndex] = packet;
    m_dirtyFlags[playerIndex].storeRelease(1);
}

// Processamento dos pacotes no loop de jogo
void GamepadManager::processLatestPackets()
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (m_dirtyFlags[i].loadAcquire() == 1) {

            const GamepadPacket& packet = m_latestPackets[i];

            // Atualização do estado do controle virtual ViGEm
            if (m_connected[i] && m_targets[i]) {
                XUSB_REPORT report;
                XUSB_REPORT_INIT(&report);

                // Mapeamento dos botões
                report.wButtons = packet.buttons;

                // Mapeamento dos gatilhos
                report.bLeftTrigger = packet.leftTrigger;
                report.bRightTrigger = packet.rightTrigger;

                // Escalonamento dos analógicos para formato XInput
                report.sThumbLX = static_cast<SHORT>(packet.leftStickX * 257);
                report.sThumbLY = static_cast<SHORT>(-packet.leftStickY * 257);
                report.sThumbRX = static_cast<SHORT>(packet.rightStickX * 257);
                report.sThumbRY = static_cast<SHORT>(-packet.rightStickY * 257);

                vigem_target_x360_update(m_client, m_targets[i], report);
            }

            // Emissão do sinal para atualização da UI
            emit gamepadStateUpdated(i, packet);

            // Reset da flag de dados novos
            m_dirtyFlags[i].storeRelease(0);
        }
    }
}

void GamepadManager::playerConnected(int playerIndex, const QString& type)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    // Criação de gamepad virtual para jogador conectado
    if (!m_connected[playerIndex]) {
        m_targets[playerIndex] = vigem_target_x360_alloc();
        const VIGEM_ERROR addResult = vigem_target_add(m_client, m_targets[playerIndex]);
        if (VIGEM_SUCCESS(addResult)) {
            vigem_target_x360_register_notification(m_client, m_targets[playerIndex],
                &GamepadManager::x360NotificationCallback, this);
            m_connected[playerIndex] = true;
            qDebug() << "Gamepad virtual criado para jogador" << playerIndex + 1 << "via" << type;
        }
        else {
            qCritical() << "Falha ao adicionar gamepad virtual para jogador" << playerIndex + 1 << ":" << addResult;
            vigem_target_free(m_targets[playerIndex]);
            m_targets[playerIndex] = nullptr;
        }
    }
    emit playerConnectedSignal(playerIndex, type);
}

void GamepadManager::playerDisconnected(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    cleanupGamepad(playerIndex);
    emit playerDisconnectedSignal(playerIndex);
}

void GamepadManager::cleanupGamepad(int playerIndex)
{
    // Remoção do gamepad virtual do jogador
    if (m_connected[playerIndex] && m_targets[playerIndex]) {
        vigem_target_remove(m_client, m_targets[playerIndex]);
        vigem_target_free(m_targets[playerIndex]);
        m_targets[playerIndex] = nullptr;
        m_connected[playerIndex] = false;
        qDebug() << "Gamepad virtual removido para jogador" << playerIndex + 1;
    }
}

// Processamento dos comandos de vibração
void GamepadManager::handleVibration(int playerIndex, UCHAR largeMotor, UCHAR smallMotor)
{
    if (largeMotor > 0 || smallMotor > 0) {
        int duration = std::min(std::max(static_cast<int>(largeMotor), static_cast<int>(smallMotor)) * 2, 500);
        QByteArray command = QByteArray("{\"type\":\"vibration\",\"pattern\":[") + QByteArray::number(duration) + QByteArray("]}");
        emit vibrationCommandReady(playerIndex, command);
    }
}