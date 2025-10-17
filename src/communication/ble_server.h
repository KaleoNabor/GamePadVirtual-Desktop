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

    // Envio de comando de vibração para jogador
    bool sendVibration(int playerIndex, const QByteArray& command);

public slots:
    // Início e parada do servidor BLE
    void startServer();
    void stopServer();

private slots:
    // Slots para eventos de conexão BLE
    void onClientConnected();
    void onClientDisconnected();
    void onCharacteristicWritten(const QLowEnergyCharacteristic& characteristic, const QByteArray& newValue);

signals:
    // Sinais para comunicação externa
    void packetReceived(int playerIndex, const QByteArray& packetData);
    void playerConnected(int playerIndex, const QString& type);
    void playerDisconnected(int playerIndex);
    void logMessage(const QString& message);

private:
    // Configuração do serviço BLE
    void setupService();

    // Controlador BLE principal
    QLowEnergyController* m_bleController = nullptr;
    // Serviço do gamepad
    QLowEnergyService* m_gamepadService = nullptr;
    // Característica de vibração
    QLowEnergyCharacteristic m_vibrationCharacteristic;

    // Mapeamento de clientes para slots de jogador
    QHash<QBluetoothAddress, int> m_clientPlayerMap;
    // Array de slots de jogador
    bool m_playerSlots[4];
    // Busca de slot vazio
    int findEmptySlot() const;
};

#endif