#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#include <QObject>
#include <QtBluetooth/QLowEnergyController>
#include <QtBluetooth/QLowEnergyService>
#include <QtBluetooth/QLowEnergyCharacteristic>
#include <QtBluetooth/QBluetoothAddress>
#include <QTimer>

// Classe principal do servidor BLE (Bluetooth Low Energy) para gerenciar conex�es de jogadores
class BleServer : public QObject
{
    Q_OBJECT

public:
    explicit BleServer(QObject* parent = nullptr);
    ~BleServer();

    // --- SE��O: M�TODOS P�BLICOS ---

    // Envio de comando de vibra��o para jogador
    bool sendVibration(int playerIndex, const QByteArray& command);

public slots:
    // --- SE��O: SLOTS P�BLICOS ---

    // In�cio e parada do servidor BLE
    void startServer();
    void stopServer();

    // --- MODIFICA��O ADICIONADA (REQ 2 FIX) ---
    // For�a a desconex�o de um jogador espec�fico via BLE
    void forceDisconnectPlayer(int playerIndex);

private slots:
    // --- SE��O: SLOTS PRIVADOS ---

    // Slots para eventos de conex�o BLE
    void onClientConnected();      // Gerencia novas conex�es de clientes
    void onClientDisconnected();   // Trata desconex�es de clientes
    void onCharacteristicWritten(const QLowEnergyCharacteristic& characteristic, const QByteArray& newValue); // Processa dados recebidos

signals:
    // --- SE��O: SINAIS ---

    // Sinais para comunica��o externa
    void packetReceived(int playerIndex, const QByteArray& packetData);  // Dados recebidos do gamepad
    void playerConnected(int playerIndex, const QString& type);          // Novo jogador conectado
    void playerDisconnected(int playerIndex);                            // Jogador desconectado
    void logMessage(const QString& message);                             // Mensagens de log

private:
    // --- SE��O: M�TODOS PRIVADOS ---

    // Configura��o do servi�o BLE e caracter�sticas
    void setupService();

    // --- SE��O: MEMBROS PRIVADOS ---

    // Controlador BLE principal (GATT Server)
    QLowEnergyController* m_bleController = nullptr;
    // Servi�o do gamepad (cont�m caracter�sticas de entrada/sa�da)
    QLowEnergyService* m_gamepadService = nullptr;
    // Caracter�stica de vibra��o (para feedback h�ptico)
    QLowEnergyCharacteristic m_vibrationCharacteristic;

    // Mapeamento de endere�os de clientes para �ndices de jogador
    QHash<QBluetoothAddress, int> m_clientPlayerMap;
    // Array de slots de jogador ocupados (8 jogadores m�ximo)
    bool m_playerSlots[8];
    // Busca de slot vazio para novo jogador
    int findEmptySlot() const;
};

#endif