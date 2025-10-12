#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <QObject>
#include <QThread>
#include "wifi_server.h"      // Servidor de Descoberta (UDP)
#include "tcp_server.h"       // Servidor de Dados (TCP)
#include "bluetooth_server.h" // Servidor Bluetooth (ainda presente)
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

    // Threads para cada servidor
    QThread m_discoveryThread;  // Thread para o servidor de descoberta UDP
    QThread m_tcpDataThread;    // Thread para o servidor de dados TCP
    QThread m_bluetoothThread;

    // Ponteiros para os workers que rodam nas threads
    WifiServer* m_discoveryServer; // Renomeado para clareza
    TcpServer* m_tcpServer;
    BluetoothServer* m_bluetoothServer;
};

#endif // CONNECTION_MANAGER_H