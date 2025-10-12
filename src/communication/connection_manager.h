#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <QObject>
#include <QThread>
#include "wifi_server.h"      // Servidor de Descoberta (UDP)
#include "udp_server.h"       // <<< NOVO SERVIDOR DE DADOS (UDP)
#include "bluetooth_server.h"
#include "../virtual_gamepad/gamepad_manager.h"

class ConnectionManager : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionManager(GamepadManager* gamepadManager, QObject* parent = nullptr);
    ~ConnectionManager();

public slots:
    void startServices();
    void stopServices();

signals:
    void logMessage(const QString& message);

private:
    GamepadManager* m_gamepadManager;

    // Threads
    QThread m_discoveryThread;
    QThread m_udpDataThread;      // <<< RENOMEADO DE m_tcpDataThread
    QThread m_bluetoothThread;

    // Ponteiros para os workers
    WifiServer* m_discoveryServer;
    UdpServer* m_udpServer;          // <<< RENOMEADO DE m_tcpServer
    BluetoothServer* m_bluetoothServer;
};

#endif // CONNECTION_MANAGER_H