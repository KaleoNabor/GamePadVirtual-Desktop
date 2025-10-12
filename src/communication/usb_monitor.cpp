#include "usb_monitor.h"
#include <QSerialPortInfo>
#include <QDebug>

const QByteArray HANDSHAKE_QUERY = "GAMEPAD_VIRTUAL_PC_QUERY";
const QByteArray HANDSHAKE_ACK = "GAMEPAD_VIRTUAL_APP_ACK";

UsbMonitor::UsbMonitor(QObject* parent) : QObject(parent)
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        m_playerSlots[i] = false;
    }

    m_scanTimer = new QTimer(this);
    connect(m_scanTimer, &QTimer::timeout, this, &UsbMonitor::checkForNewDevices);
}

UsbMonitor::~UsbMonitor()
{
    stopMonitoring();
}

void UsbMonitor::startMonitoring()
{
    if (!m_scanTimer->isActive()) {
        qDebug() << "Iniciando monitoramento de portas USB/Serial...";
        emit logMessage("Monitor USB iniciado.");
        m_scanTimer->start(2000);
        checkForNewDevices();
    }
}

void UsbMonitor::stopMonitoring()
{
    m_scanTimer->stop();
    qDeleteAll(m_connectedPorts);
    m_connectedPorts.clear();
    m_portPlayerMap.clear();
    qDebug() << "Monitoramento USB parado.";
}

void UsbMonitor::checkForNewDevices()
{
    const auto serialPortInfos = QSerialPortInfo::availablePorts();

    for (const QSerialPortInfo& portInfo : serialPortInfos) {

        // =========================================================================
        // MUDANÇA FINAL: Filtro por Vendor ID (VID)
        // =========================================================================
        // Esta é a forma mais robusta. Portas seriais físicas/antigas do sistema
        // geralmente não têm um Vendor ID, enquanto qualquer dispositivo USB real (como o celular) terá.
        if (!portInfo.hasVendorIdentifier()) {
            qDebug() << "Ignorando porta" << portInfo.portName() << " (sem Vendor ID). Provavelmente uma porta do sistema.";
            continue; // Pula para a próxima porta da lista
        }

        // Adicionalmente, podemos verificar se o fabricante é conhecido por fazer portas seriais de PC.
        // Ex: FTDI, Prolific. Se for, podemos ignorar também. (Opcional)
        // if (portInfo.manufacturer().contains("FTDI", Qt::CaseInsensitive)) {
        //     continue;
        // }


        bool alreadyConnected = false;
        for (const QSerialPort* port : qAsConst(m_connectedPorts)) {
            if (port->portName() == portInfo.portName()) {
                alreadyConnected = true;
                break;
            }
        }

        if (!alreadyConnected) {
            if (findEmptySlot() == -1) {
                continue;
            }

            QSerialPort* serialPort = new QSerialPort(this);
            serialPort->setPortName(portInfo.portName());

            if (serialPort->open(QIODevice::ReadWrite)) {
                serialPort->setBaudRate(QSerialPort::Baud115200);

                connect(serialPort, &QSerialPort::readyRead, this, &UsbMonitor::handleHandshake);
                connect(serialPort, &QSerialPort::errorOccurred, this, &UsbMonitor::handleSerialError);

                serialPort->write(HANDSHAKE_QUERY);
                qDebug() << "Porta" << portInfo.portName() << "(Fabr:" << portInfo.manufacturer() << ") aberta. Enviando query de handshake...";

                QTimer::singleShot(2500, this, [=]() { // Aumentei um pouco o timeout
                    if (m_portPlayerMap.find(serialPort) == m_portPlayerMap.end() && serialPort->isOpen()) {
                        qDebug() << "Timeout de handshake para" << portInfo.portName() << ". Fechando porta.";
                        serialPort->close();
                        serialPort->deleteLater();
                    }
                    });

            }
            else {
                delete serialPort;
            }
        }
    }
}

// O resto do arquivo (handleHandshake, readSocket, etc.) continua o mesmo
void UsbMonitor::handleHandshake()
{
    QSerialPort* port = qobject_cast<QSerialPort*>(sender());
    if (!port) return;

    if (m_portPlayerMap.contains(port)) return;

    if (port->bytesAvailable() >= HANDSHAKE_ACK.size()) {
        QByteArray response = port->read(HANDSHAKE_ACK.size());

        if (response == HANDSHAKE_ACK) {
            int playerIndex = findEmptySlot();
            if (playerIndex != -1) {
                qDebug() << "Handshake bem-sucedido com" << port->portName() << "para Jogador" << (playerIndex + 1);

                disconnect(port, &QSerialPort::readyRead, this, &UsbMonitor::handleHandshake);
                connect(port, &QSerialPort::readyRead, this, &UsbMonitor::readSocket);

                m_connectedPorts.append(port);
                m_playerSlots[playerIndex] = true;
                m_portPlayerMap[port] = playerIndex;
                emit playerConnected(playerIndex, "USB");
            }
        }
        else {
            qDebug() << "Handshake falhou com" << port->portName() << ". Resposta inválida. Fechando.";
            port->close();
        }
    }
}

void UsbMonitor::readSocket()
{
    QSerialPort* port = qobject_cast<QSerialPort*>(sender());
    if (!port || !m_portPlayerMap.contains(port)) return;

    int playerIndex = m_portPlayerMap.value(port);

    while (port->bytesAvailable() >= static_cast<qint64>(sizeof(GamepadPacket))) {
        QByteArray data = port->read(sizeof(GamepadPacket));
        const GamepadPacket* packet = reinterpret_cast<const GamepadPacket*>(data.constData());
        emit packetReceived(playerIndex, *packet);
    }
}

void UsbMonitor::handleSerialError(QSerialPort::SerialPortError error)
{
    if (error == QSerialPort::ResourceError) {
        QSerialPort* port = qobject_cast<QSerialPort*>(sender());
        if (port) {
            qDebug() << "Dispositivo USB" << port->portName() << "desconectado.";
            closeAndRemovePort(port);
        }
    }
}

void UsbMonitor::closeAndRemovePort(QSerialPort* port)
{
    if (m_portPlayerMap.contains(port)) {
        int playerIndex = m_portPlayerMap.value(port);
        m_playerSlots[playerIndex] = false;
        m_portPlayerMap.remove(port);
        m_connectedPorts.removeAll(port);
        port->close();
        port->deleteLater();
        emit playerDisconnected(playerIndex);
    }
}

int UsbMonitor::findEmptySlot() const
{
    for (int i = 0; i < MAX_PLAYERS; ++i) {
        if (!m_playerSlots[i]) {
            return i;
        }
    }
    return -1;
}