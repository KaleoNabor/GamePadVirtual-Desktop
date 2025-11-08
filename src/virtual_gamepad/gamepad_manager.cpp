// gamepad_manager.cpp (VERSÃO FINAL OTIMIZADA)

#define NOMINMAX
#include "gamepad_manager.h"
#include <QDebug>
#include <cmath>
#include <algorithm>
#include <QVector>
#include <QNetworkInterface>
#include <QtEndian>

// =============================================
// FUNÇÕES AUXILIARES
// =============================================

// Função de cálculo CRC32
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

// Converte QByteArray em string hexadecimal para logging
QString bytesToHex(const QByteArray& bytes) {
    QString hexString;
    for (const char& byte : bytes) {
        hexString.append(QString("%1").arg(static_cast<unsigned char>(byte), 2, 16, QChar('0')).toUpper());
        hexString.append(" ");
    }
    return hexString.trimmed();
}

// =============================================
// CALLBACK DE VIBRAÇÃO
// =============================================

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

// =============================================
// CONSTRUTOR E DESTRUTOR
// =============================================

GamepadManager::GamepadManager(QObject* parent)
    : QObject(parent), m_client(nullptr), m_cemuhookClientSubscribed(false), m_cemuhookClientPort(0)
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_targets[i] = nullptr;
        m_connected[i] = false;
        m_dirtyFlags[i].storeRelease(0);
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
        qDebug() << "✅ Socket CemuhookUDP vinculado na porta" << m_cemuhookPort << "(Modo IPv4)";

        QList<QHostAddress> ipAddressesList = QNetworkInterface::allAddresses();
        for (const QHostAddress& address : ipAddressesList) {
            if (address.protocol() == QAbstractSocket::IPv4Protocol && address != QHostAddress::LocalHost) {
                qDebug() << "📡 Endereço de rede disponível:" << address.toString();
            }
        }

        connect(m_cemuhookSocket, &QUdpSocket::readyRead, this, &GamepadManager::readPendingCemuhookDatagrams);
    }
    else {
        qCritical() << "❌ Falha ao vincular socket CemuhookUDP na porta" << m_cemuhookPort;
        qCritical() << "💡 Verifique se a porta não está sendo usada por outro programa";
    }
}

GamepadManager::~GamepadManager()
{
    shutdown();
}

// =============================================
// INICIALIZAÇÃO E FINALIZAÇÃO
// =============================================

bool GamepadManager::initialize()
{
    m_client = vigem_alloc();
    if (m_client == nullptr) {
        qCritical() << "❌ Falha ao alocar cliente ViGEm";
        return false;
    }
    const VIGEM_ERROR retval = vigem_connect(m_client);
    if (!VIGEM_SUCCESS(retval)) {
        qCritical() << "❌ Falha ao conectar com ViGEm:" << retval;
        vigem_free(m_client);
        m_client = nullptr;
        return false;
    }
    qDebug() << "✅ ViGEm inicializado com sucesso";
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

// =============================================
// PROCESSAMENTO DE PACOTES DE ENTRADA
// =============================================

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

            emit gamepadStateUpdated(i, packet);
            m_dirtyFlags[i].storeRelease(0);
        }
    }

    // Timeout do cliente DSU
    if (m_cemuhookClientSubscribed && m_cemuhookClientTimer.elapsed() > 5000) {
        qDebug() << "⏰ Cemuhook: Cliente DSU timed out. Parando stream.";
        m_cemuhookClientSubscribed = false;
    }

    // Envio de dados DSU para cliente inscrito
    if (!m_cemuhookClientSubscribed) return;

    for (int i = 0; i < DSU_MAX_CONTROLLERS; ++i)
    {
        if (m_connected[i])
        {
            const GamepadPacket& packet = m_latestPackets[i];
            QByteArray dsuPacket;
            dsuPacket.resize(100);
            dsuPacket.fill(0);

            // Header
            dsuPacket[0] = 'D'; dsuPacket[1] = 'S'; dsuPacket[2] = 'U'; dsuPacket[3] = 'S';
            *reinterpret_cast<quint16*>(dsuPacket.data() + 4) = qToLittleEndian<quint16>(1001);
            *reinterpret_cast<quint16*>(dsuPacket.data() + 6) = qToLittleEndian<quint16>(84);
            *reinterpret_cast<quint32*>(dsuPacket.data() + 12) = qToLittleEndian<quint32>(0);

            // Payload
            *reinterpret_cast<quint32*>(dsuPacket.data() + 16) = qToLittleEndian<quint32>(0x100002);
            dsuPacket[20] = i;
            dsuPacket[21] = 2;
            dsuPacket[22] = 2;
            dsuPacket[23] = 1;

            // MAC Address
            dsuPacket[24] = 0xAA;
            dsuPacket[25] = 0xBB;
            dsuPacket[26] = 0xCC;
            dsuPacket[27] = 0xDD;
            dsuPacket[28] = 0xEE;
            dsuPacket[29] = 0xFF;

            dsuPacket[30] = 5; // Bateria
            dsuPacket[31] = 0;

            *reinterpret_cast<quint32*>(dsuPacket.data() + 32) = qToLittleEndian<quint32>(m_dsuPacketCounter[i]++);

            // Botões - Formato DSU correto
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

            dsuPacket[38] = (packet.buttons & START) ? 1 : 0; // PS Button
            dsuPacket[39] = 0; // Touch

            // Analógicos
            dsuPacket[40] = static_cast<quint8>(packet.leftStickX + 128);
            dsuPacket[41] = static_cast<quint8>(-packet.leftStickY + 128);
            dsuPacket[42] = static_cast<quint8>(packet.rightStickX + 128);
            dsuPacket[43] = static_cast<quint8>(-packet.rightStickY + 128);

            // Gatilhos analógicos
            dsuPacket[54] = packet.rightTrigger;
            dsuPacket[55] = packet.leftTrigger;

            // Timestamp
            *reinterpret_cast<quint64*>(dsuPacket.data() + 68) = qToLittleEndian<quint64>(m_cemuhookClientTimer.nsecsElapsed() / 1000);

            // Acelerômetro e Giroscópio
            *reinterpret_cast<float*>(dsuPacket.data() + 76) = (packet.accelX / 4096.0f);
            *reinterpret_cast<float*>(dsuPacket.data() + 80) = (packet.accelY / 4096.0f);
            *reinterpret_cast<float*>(dsuPacket.data() + 84) = (packet.accelZ / 4096.0f);
            *reinterpret_cast<float*>(dsuPacket.data() + 88) = static_cast<float>(packet.gyroX / 100.0);
            *reinterpret_cast<float*>(dsuPacket.data() + 92) = static_cast<float>(packet.gyroY / 100.0);
            *reinterpret_cast<float*>(dsuPacket.data() + 96) = static_cast<float>(packet.gyroZ / 100.0);

            // CRC32
            quint32 calculated_crc = crc32(
                reinterpret_cast<const unsigned char*>(dsuPacket.constData()),
                dsuPacket.size()
            );
            *reinterpret_cast<quint32*>(dsuPacket.data() + 8) = calculated_crc;

            // Log apenas do primeiro pacote
            if (m_dsuPacketCounter[i] == 1) {
                qDebug() << "📦 Primeiro pacote DSU enviado - Contador:" << m_dsuPacketCounter[i];
                qDebug() << "   Endereço:" << m_cemuhookClientAddress.toString() << "Porta:" << m_cemuhookClientPort;
                qDebug() << "   CRC32 Calculado:" << QString::number(calculated_crc, 16);
            }

            // Envio do pacote
            qint64 bytesSent = m_cemuhookSocket->writeDatagram(dsuPacket, m_cemuhookClientAddress, m_cemuhookClientPort);

            if (bytesSent == -1) {
                qDebug() << "❌ Erro ao enviar pacote DSU:" << m_cemuhookSocket->errorString();
            }
        }
    }
}

// =============================================
// PROCESSAMENTO DE PACOTES CEMUHOOK
// =============================================

void GamepadManager::readPendingCemuhookDatagrams()
{
    while (m_cemuhookSocket->hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_cemuhookSocket->pendingDatagramSize());
        QHostAddress senderAddress;
        quint16 senderPort;

        qint64 bytesRead = m_cemuhookSocket->readDatagram(datagram.data(), datagram.size(), &senderAddress, &senderPort);

        // Filtro de pacotes inválidos
        if (bytesRead <= 0 || senderAddress.isNull() || senderPort == 0) {
            continue; // Ignora pacotes de ruído
        }

        if (bytesRead < 16) {
            qDebug() << "❌ Pacote muito pequeno, ignorando";
            continue;
        }

        if (datagram[0] != 'D' || datagram[1] != 'S' || datagram[2] != 'U' || datagram[3] != 'C') {
            qDebug() << "❌ Magic bytes inválidos, ignorando pacote";
            continue;
        }

        quint16 version = *reinterpret_cast<const quint16*>(datagram.data() + 4);
        if (version != 1001) {
            qDebug() << "❌ Versão do protocolo inválida:" << version << "(esperado: 1001)";
            continue;
        }

        m_cemuhookClientTimer.restart();

        quint32 requestType = *reinterpret_cast<const quint32*>(datagram.data() + 16);
        quint32 packetId = *reinterpret_cast<const quint32*>(datagram.data() + 12);

        qDebug() << "📥 Pacote de" << senderAddress.toString() << ":" << senderPort
            << "Tipo:" << QString::number(requestType, 16) << "ID:" << packetId;

        QByteArray responseHeader;
        responseHeader.resize(16);
        responseHeader.fill(0);
        responseHeader[0] = 'D'; responseHeader[1] = 'S'; responseHeader[2] = 'U'; responseHeader[3] = 'S';
        *reinterpret_cast<quint16*>(responseHeader.data() + 4) = 1001;
        *reinterpret_cast<quint32*>(responseHeader.data() + 12) = packetId;

        if (requestType == 0x100000)
        {
            qDebug() << "🔧 Cliente solicitou VERSÃO";
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
            qDebug() << "🔧 Cliente solicitou INFORMAÇÕES";

            int slotCount = 4;
            QVector<int> requestedSlots;

            if (bytesRead >= 24) {
                slotCount = *reinterpret_cast<const quint32*>(datagram.data() + 20);
                slotCount = std::min(slotCount, 4);
            }
            if (bytesRead >= 25) {
                for (int i = 0; i < slotCount && (24 + i) < bytesRead; i++) {
                    int slot = static_cast<unsigned char>(datagram[24 + i]);
                    if (slot < 4) requestedSlots.append(slot);
                }
            }

            if (requestedSlots.isEmpty()) {
                for (int i = 0; i < slotCount; i++) requestedSlots.append(i);
            }

            for (int slotIndex : requestedSlots) {

                bool isConnected = (slotIndex < DSU_MAX_CONTROLLERS) && m_connected[slotIndex];

                QByteArray response = responseHeader;
                response.resize(28);

                // Limpeza de memória
                memset(response.data() + 16, 0, 12);

                *reinterpret_cast<quint16*>(response.data() + 6) = 12;

                response[16] = slotIndex;
                response[17] = isConnected ? 2 : 0;
                response[18] = isConnected ? 2 : 0;
                response[19] = isConnected ? 1 : 0;

                if (isConnected) {
                    response[20] = 0xAA;
                    response[21] = 0xBB;
                    response[22] = 0xCC;
                    response[23] = 0xDD;
                    response[24] = 0xEE;
                    response[25] = 0xFF;
                }

                response[26] = isConnected ? 5 : 0;

                quint32 crc = crc32(reinterpret_cast<const unsigned char*>(response.constData()), response.size());
                *reinterpret_cast<quint32*>(response.data() + 8) = crc;

                m_cemuhookSocket->writeDatagram(response, senderAddress, senderPort);
            }

            // Inscrição automática para streaming
            if (!m_cemuhookClientSubscribed) {
                qDebug() << "🎮 CLIENTE DSU INSCRITO:" << senderAddress.toString() << ":" << senderPort;
                qDebug() << "   📡 Iniciando streaming de dados...";
            }
            m_cemuhookClientAddress = senderAddress;
            m_cemuhookClientPort = senderPort;
            m_cemuhookClientSubscribed = true;
        }
        else if (requestType == 0x100002)
        {
            if (!m_cemuhookClientSubscribed) {
                qDebug() << "🎮 CLIENTE DSU INSCRITO (via DADOS):" << senderAddress.toString() << ":" << senderPort;
            }

            m_cemuhookClientAddress = senderAddress;
            m_cemuhookClientPort = senderPort;
            m_cemuhookClientSubscribed = true;

            for (int i = 0; i < DSU_MAX_CONTROLLERS; ++i) m_dsuPacketCounter[i] = 0;
        }
        else
        {
            qDebug() << "⚠️  Tipo de requisição desconhecido:" << QString::number(requestType, 16);
        }
    }
}

// =============================================
// GERENCIAMENTO DE CONTROLES VIRTUAIS
// =============================================

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
            qDebug() << "🎮 Gamepad virtual XBOX criado para jogador" << playerIndex + 1;
        }
        else {
            qCritical() << "❌ Falha ao adicionar gamepad XBOX para jogador" << playerIndex + 1;
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
        qDebug() << "🎮 Gamepad virtual removido para jogador" << playerIndex + 1;
    }
}

// =============================================
// VIBRAÇÃO E UTILITÁRIOS
// =============================================

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

void GamepadManager::printServerStatus()
{
    qDebug() << "=== STATUS DO SERVIDOR DSU ===";
    qDebug() << "📡 Socket vinculado:" << (m_cemuhookSocket->state() == QUdpSocket::BoundState);
    qDebug() << "🎮 Cliente inscrito:" << m_cemuhookClientSubscribed;
    qDebug() << "⏰ Timer ativo:" << (m_cemuhookClientTimer.isValid() ? "Sim" : "Não");

    if (m_cemuhookClientSubscribed) {
        qDebug() << "   Endereço do cliente:" << m_cemuhookClientAddress.toString();
        qDebug() << "   Porta do cliente:" << m_cemuhookClientPort;
        qDebug() << "   Tempo desde último pacote:" << m_cemuhookClientTimer.elapsed() << "ms";
    }

    qDebug() << "🎯 Controles conectados:";
    for (int i = 0; i < DSU_MAX_CONTROLLERS; ++i) {
        qDebug() << "   Slot" << i << ":" << (m_connected[i] ? "✅ Conectado" : "❌ Desconectado");
    }
    qDebug() << "===============================";
}