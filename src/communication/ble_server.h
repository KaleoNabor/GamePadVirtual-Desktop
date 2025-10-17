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

    // Envio de comando de vibra��o para jogador
    bool sendVibration(int playerIndex, const QByteArray& command);

public slots:
    // In�cio e parada do servidor BLE
    void startServer();
    void stopServer();

private slots:
    // Slots para eventos de conex�o BLE
    void onClientConnected();
    void onClientDisconnected();
    void onCharacteristicWritten(const QLowEnergyCharacteristic& characteristic, const QByteArray& newValue);

signals:
    // Sinais para comunica��o externa
    void packetReceived(int playerIndex, const QByteArray& packetData);
    void playerConnected(int playerIndex, const QString& type);
    void playerDisconnected(int playerIndex);
    void logMessage(const QString& message);

private:
    // Configura��o do servi�o BLE
    void setupService();

    // Controlador BLE principal
    QLowEnergyController* m_bleController = nullptr;
    // Servi�o do gamepad
    QLowEnergyService* m_gamepadService = nullptr;
    // Caracter�stica de vibra��o
    QLowEnergyCharacteristic m_vibrationCharacteristic;

    // Mapeamento de clientes para slots de jogador
    QHash<QBluetoothAddress, int> m_clientPlayerMap;
    // Array de slots de jogador
    bool m_playerSlots[4];
    // Busca de slot vazio
    int findEmptySlot() const;
};

#endif