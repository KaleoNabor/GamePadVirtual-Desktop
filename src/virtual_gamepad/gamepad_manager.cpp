// gamepad_manager.cpp

#define NOMINMAX
#include "gamepad_manager.h"
#include <QDebug>
#include <cmath>
#include <algorithm>

void CALLBACK GamepadManager::x360NotificationCallback(
    VigemClient Client, VigemTarget Target, UCHAR LargeMotor,
    UCHAR SmallMotor, UCHAR LedNumber, LPVOID UserData)
{
    Q_UNUSED(Client); Q_UNUSED(LedNumber);
    GamepadManager* manager = static_cast<GamepadManager*>(UserData);
    if (!manager) return;

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (manager->m_targets[i] == Target) {
            manager->handleX360Vibration(i, LargeMotor, SmallMotor);
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

    m_processingTimer = new QTimer(this);
    m_processingTimer->setInterval(8);
    connect(m_processingTimer, &QTimer::timeout, this, &GamepadManager::processLatestPackets);
    m_processingTimer->start();

    m_cemuhookSocket = new QUdpSocket(this);
    m_cemuhookHost.setAddress("127.0.0.1");
    m_cemuhookPort = 26760;
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

void GamepadManager::onPacketReceived(int playerIndex, const GamepadPacket& packet)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;
    m_latestPackets[playerIndex] = packet;
    m_dirtyFlags[playerIndex].storeRelease(1);
}

void GamepadManager::processLatestPackets()
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (m_dirtyFlags[i].loadAcquire() == 1) {

            const GamepadPacket& packet = m_latestPackets[i];

            if (m_connected[i] && m_targets[i])
            {
                XUSB_REPORT report;
                XUSB_REPORT_INIT(&report);
                report.wButtons = packet.buttons;
                report.bLeftTrigger = packet.leftTrigger;
                report.bRightTrigger = packet.rightTrigger;
                report.sThumbLX = static_cast<SHORT>(packet.leftStickX * 257);
                report.sThumbLY = static_cast<SHORT>(-packet.leftStickY * 257);
                report.sThumbRX = static_cast<SHORT>(packet.rightStickX * 257);
                report.sThumbRY = static_cast<SHORT>(-packet.rightStickY * 257);
                vigem_target_x360_update(m_client, m_targets[i], report);
            }

            if (m_connected[i])
            {
                QByteArray dsuPacket;
                dsuPacket.resize(100);
                dsuPacket.fill(0);

                dsuPacket[0] = 'D'; dsuPacket[1] = 'S'; dsuPacket[2] = 'U'; dsuPacket[3] = 'C';
                *reinterpret_cast<quint16*>(dsuPacket.data() + 4) = 1001;
                *reinterpret_cast<quint16*>(dsuPacket.data() + 6) = 100;
                *reinterpret_cast<quint32*>(dsuPacket.data() + 12) = i;

                *reinterpret_cast<float*>(dsuPacket.data() + 40) = (packet.accelX / 4096.0f);
                *reinterpret_cast<float*>(dsuPacket.data() + 44) = (packet.accelY / 4096.0f);
                *reinterpret_cast<float*>(dsuPacket.data() + 48) = (packet.accelZ / 4096.0f);

                *reinterpret_cast<float*>(dsuPacket.data() + 52) = packet.gyroX;
                *reinterpret_cast<float*>(dsuPacket.data() + 56) = packet.gyroY;
                *reinterpret_cast<float*>(dsuPacket.data() + 60) = packet.gyroZ;

                m_cemuhookSocket->writeDatagram(dsuPacket, m_cemuhookHost, m_cemuhookPort);
            }

            emit gamepadStateUpdated(i, packet);
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
            qDebug() << "Gamepad virtual XBOX criado para jogador" << playerIndex + 1;
        }
        else {
            qCritical() << "Falha ao adicionar gamepad XBOX para jogador" << playerIndex + 1;
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
        vigem_target_x360_unregister_notification(m_targets[playerIndex]);
        vigem_target_remove(m_client, m_targets[playerIndex]);
        vigem_target_free(m_targets[playerIndex]);
        m_targets[playerIndex] = nullptr;
        m_connected[playerIndex] = false;
        qDebug() << "Gamepad virtual removido para jogador" << playerIndex + 1;
    }
}

void GamepadManager::handleX360Vibration(int playerIndex, UCHAR largeMotor, UCHAR smallMotor)
{
    if (largeMotor > 0 || smallMotor > 0) {
        int duration = std::min(std::max(static_cast<int>(largeMotor), static_cast<int>(smallMotor)) * 2, 500);
        int amplitude = std::min(std::max(static_cast<int>(largeMotor), static_cast<int>(smallMotor)), 255);
        QByteArray command = QByteArray("{\"type\":\"vibration\",\"pattern\":[0,") +
            QByteArray::number(duration) + QByteArray("],\"amplitudes\":[0,") +
            QByteArray::number(amplitude) + QByteArray("]}");
        emit vibrationCommandReady(playerIndex, command);
    }
}

void GamepadManager::testVibration(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS || !m_connected[playerIndex]) return;
    handleX360Vibration(playerIndex, 255, 0);
}