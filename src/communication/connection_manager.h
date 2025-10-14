#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <QObject>
#include <QThread>
#include "wifi_server.h"
#include "udp_server.h"
#include "bluetooth_server.h" // <<< Servidor Clássico
#include "ble_server.h"       // <<< Servidor BLE
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

    // Threads para cada serviço
    QThread m_discoveryThread;
    QThread m_udpDataThread;
    QThread m_bluetoothThread;
    QThread m_bleThread;

    // Ponteiros para todos os servidores possíveis
    WifiServer* m_discoveryServer;
    UdpServer* m_udpServer;
    BluetoothServer* m_bluetoothServer;
    BleServer* m_bleServer;

    // <<< NOVA VARIÁVEL para sabermos qual modo usar >>>
    bool m_isBleSupported;
};

#endif // CONNECTION_MANAGER_H