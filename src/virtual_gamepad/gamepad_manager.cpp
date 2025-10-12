#define NOMINMAX
#include "gamepad_manager.h"
#include <QDebug>
#include <cmath>
#include <algorithm>


// Definição do callback de vibração
void CALLBACK GamepadManager::x360NotificationCallback(
    VigemClient Client, VigemTarget Target, UCHAR LargeMotor,
    UCHAR SmallMotor, UCHAR LedNumber, LPVOID UserData)
{
    Q_UNUSED(Client); Q_UNUSED(LedNumber);
    GamepadManager* manager = static_cast<GamepadManager*>(UserData);
    if (!manager) return;

    // A função static agora só encontra o jogador e repassa o trabalho
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
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_targets[i] = nullptr;
        m_connected[i] = false;
        m_dirtyFlags[i].storeRelease(0);
    }

    // Configura e inicia o nosso "game loop" de processamento a 60 FPS
    m_processingTimer = new QTimer(this);
    m_processingTimer->setInterval(8); // 8ms
    connect(m_processingTimer, &QTimer::timeout, this, &GamepadManager::processLatestPackets);
    m_processingTimer->start();
}

GamepadManager::~GamepadManager()
{
    shutdown();
}

bool GamepadManager::initialize()
{
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
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        cleanupGamepad(i);
    }
    if (m_client) {
        vigem_disconnect(m_client);
        vigem_free(m_client);
        m_client = nullptr;
    }
}

// SLOT RÁPIDO: Apenas armazena o pacote e marca a flag
void GamepadManager::onPacketReceived(int playerIndex, const GamepadPacket& packet)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    m_latestPackets[playerIndex] = packet;
    m_dirtyFlags[playerIndex].storeRelease(1); // Avisa que há dados novos
}

// SLOT DO "GAME LOOP": Processa os dados a uma taxa controlada
void GamepadManager::processLatestPackets()
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (m_dirtyFlags[i].loadAcquire() == 1) { // Verifica se há dados novos

            const GamepadPacket& packet = m_latestPackets[i];

            // 1. Atualiza o estado do controle virtual ViGEm
            if (m_connected[i] && m_targets[i]) {
                XUSB_REPORT report;
                XUSB_REPORT_INIT(&report);

                // =================================================================
                // CORREÇÃO 1: Mapeamento direto e correto dos botões
                // =================================================================
                report.wButtons = packet.buttons;

                // Gatilhos
                report.bLeftTrigger = packet.leftTrigger;
                report.bRightTrigger = packet.rightTrigger;

                // =================================================================
                // CORREÇÃO 2: Escalonamento correto dos analógicos para XInput
                // O range do XInput é de -32768 a 32767. Os valores do app (-128 a 127) precisam ser escalonados.
                // =================================================================
                report.sThumbLX = static_cast<SHORT>(packet.leftStickX * 257);  // 127 * 257 ≈ 32639
                report.sThumbLY = static_cast<SHORT>(-packet.leftStickY * 257);
                report.sThumbRX = static_cast<SHORT>(packet.rightStickX * 257);
                report.sThumbRY = static_cast<SHORT>(-packet.rightStickY * 257);

                vigem_target_x360_update(m_client, m_targets[i], report);
            }

            // 2. Emite o sinal para a UI atualizar a tela
            emit gamepadStateUpdated(i, packet);

            // 3. Reseta a flag para indicar que processamos os dados
            m_dirtyFlags[i].storeRelease(0);
        }
    }
}

void GamepadManager::playerConnected(int playerIndex, const QString& type)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

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
    if (m_connected[playerIndex] && m_targets[playerIndex]) {
        vigem_target_remove(m_client, m_targets[playerIndex]);
        vigem_target_free(m_targets[playerIndex]);
        m_targets[playerIndex] = nullptr;
        m_connected[playerIndex] = false;
        qDebug() << "Gamepad virtual removido para jogador" << playerIndex + 1;
    }
}

// DENTRO DE: gamepad_manager.cpp

void GamepadManager::handleVibration(int playerIndex, UCHAR largeMotor, UCHAR smallMotor)
{
    // Esta é uma função de membro normal, então podemos usar 'emit'
    if (largeMotor > 0 || smallMotor > 0) {
        int duration = std::min(std::max(static_cast<int>(largeMotor), static_cast<int>(smallMotor)) * 2, 500);
        QByteArray command = QByteArray("{\"type\":\"vibration\",\"pattern\":[") + QByteArray::number(duration) + QByteArray("]}");
        emit vibrationCommandReady(playerIndex, command);
    }
}