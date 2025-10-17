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
    // Início de todos os serviços de conexão
    void startServices();
    // Parada de todos os serviços de conexão
    void stopServices();

signals:
    // Sinal para mensagens de log
    void logMessage(const QString& message);

private:
    // Ponteiro para o gerenciador de gamepads
    GamepadManager* m_gamepadManager;

    // Threads para cada tipo de serviço
    QThread m_discoveryThread;
    QThread m_udpDataThread;
    QThread m_bluetoothThread;
    QThread m_bleThread;

    // Ponteiros para os servidores
    WifiServer* m_discoveryServer;
    UdpServer* m_udpServer;
    BluetoothServer* m_bluetoothServer;
    BleServer* m_bleServer;

    // Flag de suporte a BLE
    bool m_isBleSupported;
};

#endif