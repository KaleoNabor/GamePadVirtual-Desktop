#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#include <QObject>
#include <QtBluetooth/QLowEnergyController>
#include <QtBluetooth/QLowEnergyService>
#include <QtBluetooth/QLowEnergyCharacteristic>
#include <QtBluetooth/QBluetoothAddress>
#include <QTimer>

class BleServer : public QObject
{
    Q_OBJECT

public:
    explicit BleServer(QObject* parent = nullptr);
    ~BleServer();

    bool sendVibration(int playerIndex, const QByteArray& command);

public slots:
    void startServer();
    void stopServer();

private slots:
    void onClientConnected();
    void onClientDisconnected();
    void onCharacteristicWritten(const QLowEnergyCharacteristic& characteristic, const QByteArray& newValue);

signals:
    void packetReceived(int playerIndex, const QByteArray& packetData);
    void playerConnected(int playerIndex, const QString& type);
    void playerDisconnected(int playerIndex);
    void logMessage(const QString& message);

private:
    void setupService();

    QLowEnergyController* m_bleController = nullptr;
    QLowEnergyService* m_gamepadService = nullptr;
    QLowEnergyCharacteristic m_vibrationCharacteristic;

    // Acompanha qual cliente está em qual slot
    QHash<QBluetoothAddress, int> m_clientPlayerMap;
    bool m_playerSlots[4]; // Supondo MAX_PLAYERS = 4
    int findEmptySlot() const;
};

#endif // BLE_SERVER_H