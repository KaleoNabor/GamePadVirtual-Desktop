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

void CALLBACK GamepadManager::ds4NotificationCallback(
    VigemClient Client, VigemTarget Target, UCHAR LargeMotor,
    UCHAR SmallMotor, DS4_LIGHTBAR_COLOR LightbarColor, LPVOID UserData)
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
    // Atualização dos controles virtuais ViGEm
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (m_dirtyFlags[i].loadAcquire() == 1) {

            // CORREÇÃO: Verificação robusta de nullptr
            if (!m_connected[i] || !m_targets[i]) {
                m_dirtyFlags[i].storeRelease(0);
                continue;
            }

            const GamepadPacket& packet = m_latestPackets[i];
            ControllerType type = m_controllerTypes[i];

            if (type == ControllerType::Xbox360)
            {
                XUSB_REPORT report;
                XUSB_REPORT_INIT(&report);
                report.wButtons = packet.buttons;
                report.bLeftTrigger = packet.leftTrigger;
                report.bRightTrigger = packet.rightTrigger;

                // CORREÇÃO: Fazer o cast para SHORT ANTES da multiplicação
                report.sThumbLX = static_cast<SHORT>(packet.leftStickX == -128 ? -32768 : static_cast<SHORT>(packet.leftStickX) * 257);
                report.sThumbLY = static_cast<SHORT>(packet.leftStickY == -128 ? 32767 : -static_cast<SHORT>(packet.leftStickY) * 257);
                report.sThumbRX = static_cast<SHORT>(packet.rightStickX == -128 ? -32768 : static_cast<SHORT>(packet.rightStickX) * 257);
                report.sThumbRY = static_cast<SHORT>(packet.rightStickY == -128 ? 32767 : -static_cast<SHORT>(packet.rightStickY) * 257);

                // CORREÇÃO: Verificação adicional de segurança
                if (m_targets[i] && m_client) {
                    vigem_target_x360_update(m_client, m_targets[i], report);
                }
            }
            else if (type == ControllerType::DualShock4)
            {
                DS4_REPORT_EX report;
                std::memset(&report, 0, sizeof(DS4_REPORT_EX));

                report.Report.bThumbLX = static_cast<BYTE>(packet.leftStickX + 128);
                report.Report.bThumbLY = static_cast<BYTE>(packet.leftStickY + 128);
                report.Report.bThumbRX = static_cast<BYTE>(packet.rightStickX + 128);
                report.Report.bThumbRY = static_cast<BYTE>(packet.rightStickY + 128);

                report.Report.bTriggerL = packet.leftTrigger;
                report.Report.bTriggerR = packet.rightTrigger;

                USHORT ds4Buttons = 0;
                UINT dpad = 0x8;
                if (packet.buttons & DPAD_UP)    dpad = 0;
                if (packet.buttons & DPAD_UP && packet.buttons & DPAD_RIGHT) dpad = 1;
                if (packet.buttons & DPAD_RIGHT) dpad = 2;
                if (packet.buttons & DPAD_DOWN && packet.buttons & DPAD_RIGHT) dpad = 3;
                if (packet.buttons & DPAD_DOWN)  dpad = 4;
                if (packet.buttons & DPAD_DOWN && packet.buttons & DPAD_LEFT) dpad = 5;
                if (packet.buttons & DPAD_LEFT)  dpad = 6;
                if (packet.buttons & DPAD_UP && packet.buttons & DPAD_LEFT) dpad = 7;
                ds4Buttons |= (dpad & 0xF);

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
                report.Report.wGyroX = static_cast<SHORT>((packet.gyroX / APP_GYRO_SCALE) * GYRO_SCALE);
                report.Report.wGyroY = static_cast<SHORT>((packet.gyroY / APP_GYRO_SCALE) * GYRO_SCALE);
                report.Report.wGyroZ = static_cast<SHORT>((packet.gyroZ / APP_GYRO_SCALE) * GYRO_SCALE);

                const float ACCEL_SCALE = 32767.0f / 4.0f;
                const float APP_ACCEL_SCALE = 4096.0f;
                report.Report.wAccelX = static_cast<SHORT>((packet.accelX / APP_ACCEL_SCALE) * ACCEL_SCALE);
                report.Report.wAccelY = static_cast<SHORT>((packet.accelY / APP_ACCEL_SCALE) * ACCEL_SCALE);
                report.Report.wAccelZ = static_cast<SHORT>((packet.accelZ / APP_ACCEL_SCALE) * ACCEL_SCALE);

                // CORREÇÃO: Verificação adicional de segurança
                if (m_targets[i] && m_client) {
                    VIGEM_ERROR ds4Result = vigem_target_ds4_update_ex(m_client, m_targets[i], report);
                    if (!VIGEM_SUCCESS(ds4Result)) {
                        qDebug() << "Erro ao atualizar DS4:" << ds4Result;
                    }
                }
            }

            emit gamepadStateUpdated(i, packet);
            m_dirtyFlags[i].storeRelease(0);
        }
    }

    // Atualização do servidor Cemuhook DSU
    if (m_cemuhookClientSubscribed && m_cemuhookClientTimer.elapsed() > 5000) {
        qDebug() << "Cliente DSU timed out. Parando stream.";
        m_cemuhookClientSubscribed = false;
        emit dsuClientDisconnected();
    }

    if (!m_cemuhookClientSubscribed) return;

    for (int i = 0; i < DSU_MAX_CONTROLLERS; ++i)
    {
        if (m_connected[i] && m_controllerTypes[i] == ControllerType::Xbox360)
        {
            const GamepadPacket& packet = m_latestPackets[i];
            QByteArray dsuPacket;
            dsuPacket.resize(100);
            dsuPacket.fill(0);

            dsuPacket[0] = 'D'; dsuPacket[1] = 'S'; dsuPacket[2] = 'U'; dsuPacket[3] = 'S';
            *reinterpret_cast<quint16*>(dsuPacket.data() + 4) = qToLittleEndian<quint16>(1001);
            *reinterpret_cast<quint16*>(dsuPacket.data() + 6) = qToLittleEndian<quint16>(84);
            *reinterpret_cast<quint32*>(dsuPacket.data() + 12) = qToLittleEndian<quint32>(0);

            *reinterpret_cast<quint32*>(dsuPacket.data() + 16) = qToLittleEndian<quint32>(0x100002);
            dsuPacket[20] = i;
            dsuPacket[21] = 2;
            dsuPacket[22] = 2;
            dsuPacket[23] = 1;

            dsuPacket[24] = 0xAA; dsuPacket[25] = 0xBB; dsuPacket[26] = 0xCC;
            dsuPacket[27] = 0xDD; dsuPacket[28] = 0xEE; dsuPacket[29] = (0xFF + i);

            dsuPacket[30] = 5;
            dsuPacket[31] = 0;

            *reinterpret_cast<quint32*>(dsuPacket.data() + 32) = qToLittleEndian<quint32>(m_dsuPacketCounter[i]++);

            quint8 buttons1 = 0;
            if (packet.buttons & DPAD_LEFT)  buttons1 |= (1 << 0);
            if (packet.buttons & DPAD_DOWN)  buttons1 |= (1 << 1);
            if (packet.buttons & DPAD_RIGHT) buttons1 |= (1 << 2);
            if (packet.buttons & DPAD_UP)    buttons1 |= (1 << 3);
            if (packet.buttons & START)      buttons1 |= (1 << 4);
            if (packet.buttons & R3)         buttons1 |= (1 << 5);
            if (packet.buttons & L3)         buttons1 |= (1 << 6);
            if (packet.buttons & SELECT)     buttons1 |= (1 << 7);
            dsuPacket[36] = static_cast<char>(buttons1);

            quint8 buttons2 = 0;
            if (packet.buttons & Y)    buttons2 |= (1 << 0);
            if (packet.buttons & B)    buttons2 |= (1 << 1);
            if (packet.buttons & A)    buttons2 |= (1 << 2);
            if (packet.buttons & X)    buttons2 |= (1 << 3);
            if (packet.buttons & R1)   buttons2 |= (1 << 4);
            if (packet.buttons & L1)   buttons2 |= (1 << 5);
            if (packet.rightTrigger > 20) buttons2 |= (1 << 6);
            if (packet.leftTrigger > 20)  buttons2 |= (1 << 7);
            dsuPacket[37] = static_cast<char>(buttons2);

            dsuPacket[38] = (packet.buttons & START) ? 1 : 0;
            dsuPacket[39] = 0;

            dsuPacket[40] = static_cast<quint8>(packet.leftStickX + 128);
            dsuPacket[41] = static_cast<quint8>(-packet.leftStickY + 128);
            dsuPacket[42] = static_cast<quint8>(packet.rightStickX + 128);
            dsuPacket[43] = static_cast<quint8>(-packet.rightStickY + 128);

            dsuPacket[54] = packet.rightTrigger;
            dsuPacket[55] = packet.leftTrigger;

            *reinterpret_cast<quint64*>(dsuPacket.data() + 68) = qToLittleEndian<quint64>(m_cemuhookClientTimer.nsecsElapsed() / 1000);

            *reinterpret_cast<float*>(dsuPacket.data() + 76) = (packet.accelX / 4096.0f);
            *reinterpret_cast<float*>(dsuPacket.data() + 80) = (packet.accelY / 4096.0f);
            *reinterpret_cast<float*>(dsuPacket.data() + 84) = (packet.accelZ / 4096.0f);
            *reinterpret_cast<float*>(dsuPacket.data() + 88) = static_cast<float>(packet.gyroX / 100.0);
            *reinterpret_cast<float*>(dsuPacket.data() + 92) = static_cast<float>(packet.gyroY / 100.0);
            *reinterpret_cast<float*>(dsuPacket.data() + 96) = static_cast<float>(packet.gyroZ / 100.0);

            quint32 calculated_crc = crc32(
                reinterpret_cast<const unsigned char*>(dsuPacket.constData()),
                dsuPacket.size()
            );
            *reinterpret_cast<quint32*>(dsuPacket.data() + 8) = calculated_crc;

            qint64 bytesSent = m_cemuhookSocket->writeDatagram(dsuPacket, m_cemuhookClientAddress, m_cemuhookClientPort);
            if (bytesSent == -1) {
                qDebug() << "Erro ao enviar pacote DSU:" << m_cemuhookSocket->errorString();
            }
        }
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