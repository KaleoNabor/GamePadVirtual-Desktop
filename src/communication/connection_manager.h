#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <QObject>
#include <QThread>
#include "wifi_server.h"
#include "udp_server.h"
#include "bluetooth_server.h"
#include "ble_server.h" 
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
    QThread m_udpDataThread;
    QThread m_bluetoothThread;
    QThread m_bleThread; // <<< ADICIONA A NOVA THREAD PARA O BLE

    // Ponteiros para os workers
    WifiServer* m_discoveryServer;
    UdpServer* m_udpServer;
    BluetoothServer* m_bluetoothServer; // Manter o antigo por enquanto
    BleServer* m_bleServer; // <<< ADICIONA O PONTEIRO PARA O NOVO SERVIDOR
};

#endif // CONNECTION_MANAGER_H