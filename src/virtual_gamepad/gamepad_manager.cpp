// gamepad_manager.cpp

#define VIGEM_ENABLE_BUS_VERSION_1_17_X_FEATURES
#define NOMINMAX

#include "gamepad_manager.h"
#include <QDebug>
#include <cmath>
#include <algorithm>
#include <QtEndian>
#include <cstring>
#include <QVector>
#include <QNetworkInterface>

// Funções auxiliares para cálculos CRC e conversão
static quint32 crc32(const unsigned char* s, size_t n) {
    quint32 crc = 0xFFFFFFFF;
    int k;
    while (n--) {
        crc ^= *s++;
        for (k = 0; k < 8; k++)
            crc = crc & 1 ? (crc >> 1) ^ 0xedb88320 : crc >> 1;
    }
    return ~crc;
}

static QString bytesToHex(const QByteArray& bytes) {
    QString hexString;
    for (const char& byte : bytes) {
        hexString.append(QString("%1").arg(static_cast<unsigned char>(byte), 2, 16, QChar('0')).toUpper());
        hexString.append(" ");
    }
    return hexString.trimmed();
}

// Callbacks de vibração para controles ViGEm
VOID CALLBACK GamepadManager::x360NotificationCallback(
    PVIGEM_CLIENT Client, PVIGEM_TARGET Target, UCHAR LargeMotor,
    UCHAR SmallMotor, UCHAR LedNumber, PVOID UserData)
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

VOID CALLBACK GamepadManager::ds4NotificationCallback(
    PVIGEM_CLIENT Client, PVIGEM_TARGET Target, UCHAR LargeMotor,
    UCHAR SmallMotor, DS4_LIGHTBAR_COLOR LightbarColor, PVOID UserData)
{
    Q_UNUSED(Client); Q_UNUSED(LightbarColor);
    GamepadManager* manager = static_cast<GamepadManager*>(UserData);
    if (!manager) return;

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (manager->m_targets[i] == Target) {
            manager->handleDS4Vibration(i, LargeMotor, SmallMotor);
            break;
        }
    }
}

// Inicialização e configuração do gerenciador
GamepadManager::GamepadManager(QObject* parent)
    : QObject(parent), m_client(nullptr),
    m_cemuhookClientSubscribed(false), m_cemuhookClientPort(0)
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_targets[i] = nullptr;
        m_connected[i] = false;
        m_dirtyFlags[i].storeRelease(0);
        m_controllerTypes[i] = ControllerType::DualShock4;
        m_dsuPacketCounter[i] = 0;
        m_dsuLastKeepAlive[i] = 0;
    }

    m_processingTimer = new QTimer(this);
    m_processingTimer->setInterval(8);
    connect(m_processingTimer, &QTimer::timeout, this, &GamepadManager::processLatestPackets);
    m_processingTimer->start();

    m_cemuhookClientTimer.start();
    m_cemuhookSocket = new QUdpSocket(this);
    m_cemuhookPort = 26760;

    if (m_cemuhookSocket->bind(QHostAddress::AnyIPv4, m_cemuhookPort, QUdpSocket::ShareAddress)) {
        qDebug() << "Socket CemuhookUDP vinculado na porta" << m_cemuhookPort;
        QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
        for (const QHostAddress& address : ipAddressesList) {
            if (address.protocol() == QAbstractSocket::IPv4Protocol && address != QHostAddress::LocalHost) {
                qDebug() << "Endereço de rede DSU disponível:" << address.toString();
            }
        }
        connect(m_cemuhookSocket, &QUdpSocket::readyRead, this, &GamepadManager::readPendingCemuhookDatagrams);
    }
    else {
        qCritical() << "Falha ao vincular socket CemuhookUDP na porta" << m_cemuhookPort;
    }
}

GamepadManager::~GamepadManager()
{
    shutdown();
}

// Gerenciamento da conexão com ViGEm
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
    if (m_cemuhookSocket) {
        m_cemuhookSocket->close();
    }

    for (int i = 0; i < MAX_PLAYERS; ++i) {
        cleanupGamepad(i);
    }
    if (m_client) {
        vigem_disconnect(m_client);
        vigem_free(m_client);
        m_client = nullptr;
    }
}

// Controle de jogadores e tipos de controle
void GamepadManager::onControllerTypeChanged(int playerIndex, int typeIndex)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    ControllerType newType = static_cast<ControllerType>(typeIndex);

    if (m_controllerTypes[playerIndex] != newType) {
        qDebug() << "Jogador" << (playerIndex + 1) << "mudou o tipo de controle para" << (newType == ControllerType::DualShock4 ? "DualShock 4" : "Xbox 360");
        m_controllerTypes[playerIndex] = newType;

        if (m_connected[playerIndex]) {
            qDebug() << "Recriando controle para jogador" << (playerIndex + 1);
            cleanupGamepad(playerIndex);
            createGamepad(playerIndex);
        }
    }
}

void GamepadManager::onPacketReceived(int playerIndex, const GamepadPacket& packet)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    // DEBUG: Verificar se os botões estão chegando corretamente
    qDebug() << "GamepadManager - Player" << playerIndex
        << "Buttons:" << QString::number(packet.buttons, 16)
        << "A:" << (packet.buttons & A)
        << "B:" << (packet.buttons & B)
        << "X:" << (packet.buttons & X)
        << "Y:" << (packet.buttons & Y)
        << "L1:" << (packet.buttons & L1)
        << "R1:" << (packet.buttons & R1)
        << "DPAD_UP:" << (packet.buttons & DPAD_UP);

    m_latestPackets[playerIndex] = packet;
    m_dirtyFlags[playerIndex].storeRelease(1);
}

void GamepadManager::createGamepad(int playerIndex)
{
    if (m_connected[playerIndex] || m_targets[playerIndex]) {
        qWarning() << "Tentativa de criar controle para jogador" << playerIndex << "que já possui um.";
        return;
    }

    ControllerType type = m_controllerTypes[playerIndex];

    if (type == ControllerType::Xbox360)
    {
        m_targets[playerIndex] = vigem_target_x360_alloc();
        const VIGEM_ERROR addResult = vigem_target_add(m_client, m_targets[playerIndex]);
        if (VIGEM_SUCCESS(addResult)) {
            vigem_target_x360_register_notification(m_client, m_targets[playerIndex],
                &GamepadManager::x360NotificationCallback, this);
            m_connected[playerIndex] = true;
            qDebug() << "Gamepad virtual Xbox 360 criado para jogador" << playerIndex + 1;
        }
        else {
            qCritical() << "Falha ao adicionar gamepad Xbox 360";
            vigem_target_free(m_targets[playerIndex]);
            m_targets[playerIndex] = nullptr;
        }
    }
    else if (type == ControllerType::DualShock4)
    {
        m_targets[playerIndex] = vigem_target_ds4_alloc();
        const VIGEM_ERROR addResult = vigem_target_add(m_client, m_targets[playerIndex]);
        if (VIGEM_SUCCESS(addResult)) {
            vigem_target_ds4_register_notification(m_client, m_targets[playerIndex],
                &GamepadManager::ds4NotificationCallback, this);
            m_connected[playerIndex] = true;
            qDebug() << "Gamepad virtual DualShock 4 criado para jogador" << playerIndex + 1;
        }
        else {
            qCritical() << "Falha ao adicionar gamepad DualShock 4";
            vigem_target_free(m_targets[playerIndex]);
            m_targets[playerIndex] = nullptr;
        }
    }
}

void GamepadManager::playerConnected(int playerIndex, const QString& type)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    if (!m_connected[playerIndex]) {
        createGamepad(playerIndex);
    }
    emit playerConnectedSignal(playerIndex, type);
}

void GamepadManager::cleanupGamepad(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;

    if (m_connected[playerIndex] && m_targets[playerIndex]) {

        ControllerType type = m_controllerTypes[playerIndex];

        if (type == ControllerType::Xbox360) {
            vigem_target_x360_unregister_notification(m_targets[playerIndex]);
        }
        else if (type == ControllerType::DualShock4) {
            vigem_target_ds4_unregister_notification(m_targets[playerIndex]);
        }

        vigem_target_remove(m_client, m_targets[playerIndex]);
        vigem_target_free(m_targets[playerIndex]);
        m_targets[playerIndex] = nullptr;
        m_connected[playerIndex] = false;
        qDebug() << "Gamepad virtual removido para jogador" << playerIndex + 1;
    }
}

void GamepadManager::playerDisconnected(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS) return;
    cleanupGamepad(playerIndex);
    emit playerDisconnectedSignal(playerIndex);
}

// Processamento principal de pacotes para ViGEm e DSU
void GamepadManager::processLatestPackets()
{
    // Loop principal para processar pacotes novos (ViGEm e DSU)
    for (int i = 0; i < MAX_PLAYERS; ++i)
    {
        // SÓ processa se um novo pacote chegou (Conserta o "travamento")
        if (m_dirtyFlags[i].loadAcquire() == 1)
        {
            // Verificação de segurança
            if (!m_connected[i] || !m_targets[i] || !m_client) {
                m_dirtyFlags[i].storeRelease(0);
                continue;
            }

            const GamepadPacket& packet = m_latestPackets[i];
            ControllerType type = m_controllerTypes[i];

            // --- 1. ATUALIZAÇÃO DO VIGEM (Xbox 360 / DS4) ---
            if (type == ControllerType::Xbox360)
            {
                XUSB_REPORT report;
                std::memset(&report, 0, sizeof(XUSB_REPORT));
                XUSB_REPORT_INIT(&report);

                // Botões Xbox 360 - CORREÇÃO APLICADA
                report.wButtons = 0;
                if (packet.buttons & A) report.wButtons |= XUSB_GAMEPAD_A;
                if (packet.buttons & B) report.wButtons |= XUSB_GAMEPAD_B;
                if (packet.buttons & X) report.wButtons |= XUSB_GAMEPAD_X;
                if (packet.buttons & Y) report.wButtons |= XUSB_GAMEPAD_Y;
                if (packet.buttons & L1) report.wButtons |= XUSB_GAMEPAD_LEFT_SHOULDER;
                if (packet.buttons & R1) report.wButtons |= XUSB_GAMEPAD_RIGHT_SHOULDER;
                if (packet.buttons & L3) report.wButtons |= XUSB_GAMEPAD_LEFT_THUMB;
                if (packet.buttons & R3) report.wButtons |= XUSB_GAMEPAD_RIGHT_THUMB;
                if (packet.buttons & SELECT) report.wButtons |= XUSB_GAMEPAD_BACK;
                if (packet.buttons & START) report.wButtons |= XUSB_GAMEPAD_START;

                // D-Pad
                if (packet.buttons & DPAD_UP) report.wButtons |= XUSB_GAMEPAD_DPAD_UP;
                if (packet.buttons & DPAD_DOWN) report.wButtons |= XUSB_GAMEPAD_DPAD_DOWN;
                if (packet.buttons & DPAD_LEFT) report.wButtons |= XUSB_GAMEPAD_DPAD_LEFT;
                if (packet.buttons & DPAD_RIGHT) report.wButtons |= XUSB_GAMEPAD_DPAD_RIGHT;

                report.bLeftTrigger = packet.leftTrigger;
                report.bRightTrigger = packet.rightTrigger;
                report.sThumbLX = (packet.leftStickX == -128) ? -32768 : static_cast<SHORT>(packet.leftStickX * 257);
                report.sThumbLY = (packet.leftStickY == -128) ? 32767 : static_cast<SHORT>(-packet.leftStickY * 257);
                report.sThumbRX = (packet.rightStickX == -128) ? -32768 : static_cast<SHORT>(packet.rightStickX * 257);
                report.sThumbRY = (packet.rightStickY == -128) ? 32767 : static_cast<SHORT>(-packet.rightStickY * 257);

                vigem_target_x360_update(m_client, m_targets[i], report);
            }
            else if (type == ControllerType::DualShock4)
            {
                DS4_REPORT_EX report;
                std::memset(&report, 0, sizeof(DS4_REPORT_EX));

                report.Report.bThumbLX = static_cast<BYTE>(std::clamp(packet.leftStickX + 128, 0, 255));
                report.Report.bThumbLY = static_cast<BYTE>(std::clamp(packet.leftStickY + 128, 0, 255));
                report.Report.bThumbRX = static_cast<BYTE>(std::clamp(packet.rightStickX + 128, 0, 255));
                report.Report.bThumbRY = static_cast<BYTE>(std::clamp(packet.rightStickY + 128, 0, 255));
                report.Report.bTriggerL = packet.leftTrigger;
                report.Report.bTriggerR = packet.rightTrigger;

                USHORT ds4Buttons = 0;
                UINT dpad = 0x8;
                if (packet.buttons & DPAD_UP && packet.buttons & DPAD_RIGHT) dpad = 1;
                else if (packet.buttons & DPAD_DOWN && packet.buttons & DPAD_RIGHT) dpad = 3;
                else if (packet.buttons & DPAD_DOWN && packet.buttons & DPAD_LEFT) dpad = 5;
                else if (packet.buttons & DPAD_UP && packet.buttons & DPAD_LEFT) dpad = 7;
                else if (packet.buttons & DPAD_UP) dpad = 0;
                else if (packet.buttons & DPAD_RIGHT) dpad = 2;
                else if (packet.buttons & DPAD_DOWN) dpad = 4;
                else if (packet.buttons & DPAD_LEFT) dpad = 6;
                ds4Buttons |= (dpad & 0xF);

                // Botões DS4 - CORREÇÃO APLICADA
                if (packet.buttons & X) ds4Buttons |= DS4_BUTTON_SQUARE;
                if (packet.buttons & A) ds4Buttons |= DS4_BUTTON_CROSS;
                if (packet.buttons & B) ds4Buttons |= DS4_BUTTON_CIRCLE;
                if (packet.buttons & Y) ds4Buttons |= DS4_BUTTON_TRIANGLE;
                if (packet.buttons & L1) ds4Buttons |= DS4_BUTTON_SHOULDER_LEFT;
                if (packet.buttons & R1) ds4Buttons |= DS4_BUTTON_SHOULDER_RIGHT;
                if (packet.buttons & L3) ds4Buttons |= DS4_BUTTON_THUMB_LEFT;
                if (packet.buttons & R3) ds4Buttons |= DS4_BUTTON_THUMB_RIGHT;
                if (packet.buttons & SELECT) ds4Buttons |= DS4_BUTTON_SHARE;
                if (packet.buttons & START)  ds4Buttons |= DS4_BUTTON_OPTIONS;
                if (packet.leftTrigger > 20)  ds4Buttons |= DS4_BUTTON_TRIGGER_LEFT;
                if (packet.rightTrigger > 20) ds4Buttons |= DS4_BUTTON_TRIGGER_RIGHT;
                report.Report.wButtons = ds4Buttons;

                const float GYRO_SCALE = 32767.0f / 2000.0f;
                const float APP_GYRO_SCALE = 100.0f;
                const float safeGyroScale = (APP_GYRO_SCALE != 0.0f) ? APP_GYRO_SCALE : 1.0f;
                report.Report.wGyroX = static_cast<SHORT>((packet.gyroX / safeGyroScale) * GYRO_SCALE);
                report.Report.wGyroY = static_cast<SHORT>((packet.gyroY / safeGyroScale) * GYRO_SCALE);
                report.Report.wGyroZ = static_cast<SHORT>((packet.gyroZ / safeGyroScale) * GYRO_SCALE);

                const float ACCEL_SCALE = 32767.0f / 4.0f;
                const float APP_ACCEL_SCALE = 4096.0f;
                const float safeAccelScale = (APP_ACCEL_SCALE != 0.0f) ? APP_ACCEL_SCALE : 1.0f;
                report.Report.wAccelX = static_cast<SHORT>((packet.accelX / safeAccelScale) * ACCEL_SCALE);
                report.Report.wAccelY = static_cast<SHORT>((packet.accelY / safeAccelScale) * ACCEL_SCALE);
                report.Report.wAccelZ = static_cast<SHORT>((packet.accelZ / safeAccelScale) * ACCEL_SCALE);

                vigem_target_ds4_update_ex(m_client, m_targets[i], report);
            }

            // --- 2. ATUALIZAÇÃO DO CEMUHOOK DSU ---
            // Só envia se o cliente DSU estiver ouvindo E o slot for 0-3 E o tipo for Xbox
            if (m_cemuhookClientSubscribed && i < DSU_MAX_CONTROLLERS && type == ControllerType::Xbox360)
            {
                QByteArray dsuPacket;
                dsuPacket.resize(100);
                dsuPacket.fill(0);

                // Header
                dsuPacket[0] = 'D'; dsuPacket[1] = 'S'; dsuPacket[2] = 'U'; dsuPacket[3] = 'S';
                *reinterpret_cast<quint16*>(dsuPacket.data() + 4) = qToLittleEndian<quint16>(1001);
                *reinterpret_cast<quint16*>(dsuPacket.data() + 6) = qToLittleEndian<quint16>(84);
                *reinterpret_cast<quint32*>(dsuPacket.data() + 12) = qToLittleEndian<quint32>(0);
                *reinterpret_cast<quint32*>(dsuPacket.data() + 16) = qToLittleEndian<quint32>(0x100002);
                dsuPacket[20] = i;
                dsuPacket[21] = 2; dsuPacket[22] = 2; dsuPacket[23] = 1;
                dsuPacket[24] = 0xAA; dsuPacket[25] = 0xBB; dsuPacket[26] = 0xCC;
                dsuPacket[27] = 0xDD; dsuPacket[28] = 0xEE; dsuPacket[29] = (0xFF + i);
                dsuPacket[30] = 5; dsuPacket[31] = 0;
                *reinterpret_cast<quint32*>(dsuPacket.data() + 32) = qToLittleEndian<quint32>(m_dsuPacketCounter[i]++);

                // --- Byte 36 (D-Pad Digital + Sistema) ---
                quint8 buttons1 = 0;
                if (packet.buttons & DPAD_LEFT)    buttons1 |= (1 << 7);
                if (packet.buttons & DPAD_DOWN)    buttons1 |= (1 << 6);
                if (packet.buttons & DPAD_RIGHT)   buttons1 |= (1 << 5);
                if (packet.buttons & DPAD_UP)      buttons1 |= (1 << 4);
                if (packet.buttons & START)        buttons1 |= (1 << 3);
                if (packet.buttons & R3)           buttons1 |= (1 << 2);
                if (packet.buttons & L3)           buttons1 |= (1 << 1);
                if (packet.buttons & SELECT)       buttons1 |= (1 << 0);
                dsuPacket[36] = static_cast<char>(buttons1);

                // --- Byte 37 (Botões de Ação + Ombros) - CORREÇÃO APLICADA ---
                quint8 buttons2 = 0;
                if (packet.buttons & Y)            buttons2 |= (1 << 7);
                if (packet.buttons & B)            buttons2 |= (1 << 6);
                if (packet.buttons & A)            buttons2 |= (1 << 5);
                if (packet.buttons & X)            buttons2 |= (1 << 4);
                if (packet.buttons & R1)           buttons2 |= (1 << 3);
                if (packet.buttons & L1)           buttons2 |= (1 << 2);
                if (packet.rightTrigger > 20)      buttons2 |= (1 << 1);
                if (packet.leftTrigger > 20)       buttons2 |= (1 << 0);
                dsuPacket[37] = static_cast<char>(buttons2);

                // --- Byte 38 & 39 (PS / Touch) ---
                dsuPacket[38] = 0;
                dsuPacket[39] = 0;

                // --- Bytes 40-43 (Analógicos) ---
                dsuPacket[40] = static_cast<quint8>(std::clamp(packet.leftStickX + 128, 0, 255));
                dsuPacket[41] = static_cast<quint8>(std::clamp(-packet.leftStickY + 128, 0, 255));
                dsuPacket[42] = static_cast<quint8>(std::clamp(packet.rightStickX + 128, 0, 255));
                dsuPacket[43] = static_cast<quint8>(std::clamp(-packet.rightStickY + 128, 0, 255));

                // --- Bytes 44-47 (D-PAD ANALÓGICO) ---
                // D-Pad analógico - CORREÇÃO APLICADA
                dsuPacket[44] = (packet.buttons & DPAD_LEFT) ? 255 : 0;      // Left analog
                dsuPacket[45] = (packet.buttons & DPAD_DOWN) ? 255 : 0;   // Down analog  
                dsuPacket[46] = (packet.buttons & DPAD_RIGHT) ? 255 : 0;    // Right analog
                dsuPacket[47] = (packet.buttons & DPAD_UP) ? 255 : 0;    // Up analog

                // --- Bytes 48-53 (BOTÕES ANALÓGICOS) - NOVA CORREÇÃO APLICADA ---
                // Botões analógicos A, B, X, Y, L1, R1
                dsuPacket[48] = (packet.buttons & X) ? 255 : 0;           // Square (X)
                dsuPacket[49] = (packet.buttons & A) ? 255 : 0;           // Cross (A)
                dsuPacket[50] = (packet.buttons & B) ? 255 : 0;           // Circle (B)
                dsuPacket[51] = (packet.buttons & Y) ? 255 : 0;           // Triangle (Y)
                dsuPacket[52] = (packet.buttons & R1) ? 255 : 0;          // R1
                dsuPacket[53] = (packet.buttons & L1) ? 255 : 0;          // L1

                // --- Bytes 54-55 (Gatilhos Analógicos) ---
                dsuPacket[54] = packet.rightTrigger;
                dsuPacket[55] = packet.leftTrigger;

                // --- Timestamp e Sensores ---
                *reinterpret_cast<quint64*>(dsuPacket.data() + 68) = qToLittleEndian<quint64>(m_cemuhookClientTimer.nsecsElapsed() / 1000);
                const float safeAccelDivisor = 4096.0f;
                const float safeGyroDivisor = 100.0f;
                *reinterpret_cast<float*>(dsuPacket.data() + 76) = (packet.accelX / safeAccelDivisor);
                *reinterpret_cast<float*>(dsuPacket.data() + 80) = (packet.accelY / safeAccelDivisor);
                *reinterpret_cast<float*>(dsuPacket.data() + 84) = (packet.accelZ / safeAccelDivisor);
                *reinterpret_cast<float*>(dsuPacket.data() + 88) = static_cast<float>(packet.gyroX / safeGyroDivisor);
                *reinterpret_cast<float*>(dsuPacket.data() + 92) = static_cast<float>(packet.gyroY / safeGyroDivisor);
                *reinterpret_cast<float*>(dsuPacket.data() + 96) = static_cast<float>(packet.gyroZ / safeGyroDivisor);

                // --- CRC ---
                quint32 calculated_crc = crc32(
                    reinterpret_cast<const unsigned char*>(dsuPacket.constData()),
                    dsuPacket.size()
                );
                *reinterpret_cast<quint32*>(dsuPacket.data() + 8) = calculated_crc;

                // --- Envio ---
                m_cemuhookSocket->writeDatagram(dsuPacket, m_cemuhookClientAddress, m_cemuhookClientPort);
            }

            // --- 3. EMITIR SINAL E LIMPAR FLAG ---
            emit gamepadStateUpdated(i, packet);
            m_dirtyFlags[i].storeRelease(0);
        }
    }

    // --- VERIFICAÇÃO DE TIMEOUT DO DSU (Não precisa mais do loop DSU aqui) ---
    if (m_cemuhookClientSubscribed && m_cemuhookClientTimer.elapsed() > 10000) {
        qDebug() << "Cliente DSU timed out após" << m_cemuhookClientTimer.elapsed() << "ms. Parando stream.";
        m_cemuhookClientSubscribed = false;
        m_cemuhookClientAddress = QHostAddress();
        m_cemuhookClientPort = 0;
        emit dsuClientDisconnected();
    }
}

// Processamento de datagramas do protocolo Cemuhook
void GamepadManager::readPendingCemuhookDatagrams()
{
    while (m_cemuhookSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_cemuhookSocket->pendingDatagramSize());
        QHostAddress senderAddress;
        quint16 senderPort;

        qint64 bytesRead = m_cemuhookSocket->readDatagram(datagram.data(), datagram.size(), &senderAddress, &senderPort);

        if (bytesRead <= 0 || senderAddress.isNull() || senderPort == 0) continue;
        if (bytesRead < 16) continue;
        if (datagram[0] != 'D' || datagram[1] != 'S' || datagram[2] != 'U' || datagram[3] != 'C') continue;

        quint16 version = *reinterpret_cast<const quint16*>(datagram.data() + 4);
        if (version != 1001) continue;

        m_cemuhookClientTimer.restart();

        quint32 requestType = *reinterpret_cast<const quint32*>(datagram.data() + 16);
        quint32 packetId = *reinterpret_cast<const quint32*>(datagram.data() + 12);

        QByteArray responseHeader;
        responseHeader.resize(16);
        responseHeader.fill(0);
        responseHeader[0] = 'D'; responseHeader[1] = 'S'; responseHeader[2] = 'U'; responseHeader[3] = 'S';
        *reinterpret_cast<quint16*>(responseHeader.data() + 4) = 1001;
        *reinterpret_cast<quint32*>(responseHeader.data() + 12) = packetId;

        if (requestType == 0x100000)
        {
            qDebug() << "Cliente DSU solicitou VERSÃO";
            QByteArray response = responseHeader;
            response.resize(20);
            *reinterpret_cast<quint16*>(response.data() + 6) = 4;
            *reinterpret_cast<quint32*>(response.data() + 16) = 1001;
            quint32 crc = crc32(reinterpret_cast<const unsigned char*>(response.constData()), response.size());
            *reinterpret_cast<quint32*>(response.data() + 8) = crc;
            m_cemuhookSocket->writeDatagram(response, senderAddress, senderPort);
        }
        else if (requestType == 0x100001)
        {
            qDebug() << "Cliente DSU solicitou INFORMAÇÕES";
            QVector<int> requestedSlots;
            if (bytesRead >= 25) {
                for (int i = 0; (24 + i) < bytesRead; i++) {
                    int slot = static_cast<unsigned char>(datagram[24 + i]);
                    if (slot < DSU_MAX_CONTROLLERS) requestedSlots.append(slot);
                }
            }
            if (requestedSlots.isEmpty()) {
                for (int i = 0; i < DSU_MAX_CONTROLLERS; i++) requestedSlots.append(i);
            }

            for (int slotIndex : requestedSlots) {
                bool isConnected = (slotIndex < MAX_PLAYERS) && m_connected[slotIndex];
                QByteArray response = responseHeader;
                response.resize(28);
                memset(response.data() + 16, 0, 12);
                *reinterpret_cast<quint16*>(response.data() + 6) = 12;
                response[16] = slotIndex;
                response[17] = isConnected ? 2 : 0;
                response[18] = isConnected ? 2 : 0;
                response[19] = isConnected ? 1 : 0;
                if (isConnected) {
                    response[20] = 0xAA; response[21] = 0xBB; response[22] = 0xCC;
                    response[23] = 0xDD; response[24] = 0xEE; response[25] = (0xFF + slotIndex);
                }
                response[26] = isConnected ? 5 : 0;
                quint32 crc = crc32(reinterpret_cast<const unsigned char*>(response.constData()), response.size());
                *reinterpret_cast<quint32*>(response.data() + 8) = crc;
                m_cemuhookSocket->writeDatagram(response, senderAddress, senderPort);
            }
        }
        else if (requestType == 0x100002)
        {
            if (!m_cemuhookClientSubscribed) {
                qDebug() << "CLIENTE DSU INSCRITO:" << senderAddress.toString() << ":" << senderPort;
                qDebug() << "Iniciando streaming de dados DSU...";
                emit dsuClientConnected(senderAddress.toString(), senderPort);
            }
            m_cemuhookClientAddress = senderAddress;
            m_cemuhookClientPort = senderPort;
            m_cemuhookClientSubscribed = true;
            for (int i = 0; i < DSU_MAX_CONTROLLERS; ++i) m_dsuPacketCounter[i] = 0;
        }
        else {
            qDebug() << "Tipo de requisição DSU desconhecido:" << QString::number(requestType, 16);
        }
    }
}

// Sistema de vibração e utilitários
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

void GamepadManager::handleDS4Vibration(int playerIndex, UCHAR largeMotor, UCHAR smallMotor)
{
    handleX360Vibration(playerIndex, largeMotor, smallMotor);
}

void GamepadManager::testVibration(int playerIndex)
{
    if (playerIndex < 0 || playerIndex >= MAX_PLAYERS || !m_connected[playerIndex]) return;

    ControllerType type = m_controllerTypes[playerIndex];
    if (type == ControllerType::Xbox360) {
        handleX360Vibration(playerIndex, 255, 0);
    }
    else if (type == ControllerType::DualShock4) {
        handleDS4Vibration(playerIndex, 255, 0);
    }
}

void GamepadManager::printServerStatus()
{
    qDebug() << "=== STATUS DO SERVIDOR ===";
    qDebug() << "Socket DSU vinculado:" << (m_cemuhookSocket->state() == QUdpSocket::BoundState) << "Porta:" << m_cemuhookPort;
    qDebug() << "Cliente DSU inscrito:" << m_cemuhookClientSubscribed;

    if (m_cemuhookClientSubscribed) {
        qDebug() << "Endereço do cliente:" << m_cemuhookClientAddress.toString();
        qDebug() << "Porta do cliente:" << m_cemuhookClientPort;
        qDebug() << "Tempo desde último pacote:" << m_cemuhookClientTimer.elapsed() << "ms";
    }

    qDebug() << "Controles ViGEm conectados:";
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        qDebug() << "Slot" << i << ":" << (m_connected[i] ? "Conectado" : "Desconectado")
            << "Tipo:" << (m_controllerTypes[i] == ControllerType::Xbox360 ? "Xbox 360" : "DualShock 4");
    }
    qDebug() << "===============================";
}