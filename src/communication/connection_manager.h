#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <QObject>
#include "network_server.h"
#include "bluetooth_server.h" 
#include "ble_server.h"
#include "../virtual_gamepad/gamepad_manager.h"

// --- ADIÇÃO: Include para a struct do pacote ---
#include "../protocol/gamepad_packet.h" 

class GamepadManager;
class NetworkServer;
class BluetoothServer;
class BleServer;

class ConnectionManager : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionManager(GamepadManager* gamepadManager, QObject* parent = nullptr);
    ~ConnectionManager();

public slots:
    void startServices();
    void stopServices();
    void forceDisconnectPlayer(int playerIndex, const QString& type);

private slots:
    void onVibrationCommandReady(int playerIndex, const QByteArray& command);

    // --- ADIÇÃO: Novo slot "tradutor" para BLE ---
    void onBlePacketReceived(int playerIndex, const QByteArray& packet);
    // --- FIM DA ADIÇÃO ---

signals:
    void logMessage(const QString& message);
    void playerConnected(int playerIndex, const QString& type);
    void playerDisconnected(int playerIndex);

private:
    GamepadManager* m_gamepadManager;
    NetworkServer* m_networkServer;
    BluetoothServer* m_bluetoothServer;
    BleServer* m_bleServer;
};

#endif