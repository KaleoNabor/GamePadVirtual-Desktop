#ifndef USB_MONITOR_H
#define USB_MONITOR_H

#include <QObject>
#include <QTimer>
#include <QSerialPort>
#include <QList>
#include <QHash>
#include "../protocol/gamepad_packet.h"

#define MAX_PLAYERS 4

class UsbMonitor : public QObject
{
    Q_OBJECT

public:
    explicit UsbMonitor(QObject* parent = nullptr);
    ~UsbMonitor();

public slots:
    void startMonitoring();
    void stopMonitoring();

private slots:
    void checkForNewDevices();
    void readSocket();
    void handleHandshake(); // NOVO SLOT para o handshake
    void handleSerialError(QSerialPort::SerialPortError error);

signals:
    void packetReceived(int playerIndex, const GamepadPacket& packet);
    void playerConnected(int playerIndex, const QString& type);
    void playerDisconnected(int playerIndex);
    void logMessage(const QString& message);

private:
    int findEmptySlot() const;
    void closeAndRemovePort(QSerialPort* port);

    QTimer* m_scanTimer;
    QList<QSerialPort*> m_connectedPorts;
    QHash<QSerialPort*, int> m_portPlayerMap;
    bool m_playerSlots[MAX_PLAYERS];
};

#endif // USB_MONITOR_H