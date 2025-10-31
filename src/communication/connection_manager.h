#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <QObject>
#include <QThread>
#include "wifi_server.h"
#include "udp_server.h"
#include "bluetooth_server.h"
#include "ble_server.h"
#include "../virtual_gamepad/gamepad_manager.h"

// Classe principal para gerenciar todos os servi�os de conex�o
class ConnectionManager : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionManager(GamepadManager* gamepadManager, QObject* parent = nullptr);
    ~ConnectionManager();

public slots:
    // --- SE��O: SLOTS P�BLICOS ---

    // In�cio de todos os servi�os de conex�o
    void startServices();
    // Parada de todos os servi�os de conex�o
    void stopServices();

    // --- MODIFICA��O ADICIONADA (REQ 2 FIX) ---
    // For�a a desconex�o de um jogador espec�fico por tipo de conex�o
    void forceDisconnectPlayer(int playerIndex, const QString& type);

signals:
    // --- SE��O: SINAIS ---

    // Sinal para mensagens de log
    void logMessage(const QString& message);

private:
    // --- SE��O: MEMBROS PRIVADOS ---

    // Ponteiro para o gerenciador de gamepads
    GamepadManager* m_gamepadManager;

    // Threads para cada tipo de servi�o (execu��o em paralelo)
    QThread m_discoveryThread;    // Thread para servidor de descoberta Wi-Fi
    QThread m_udpDataThread;      // Thread para servidor de dados UDP
    QThread m_bluetoothThread;    // Thread para servidor Bluetooth cl�ssico
    QThread m_bleThread;          // Thread para servidor Bluetooth LE

    // Ponteiros para os servidores de cada protocolo
    WifiServer* m_discoveryServer;    // Servidor de descoberta Wi-Fi
    UdpServer* m_udpServer;           // Servidor de dados UDP
    BluetoothServer* m_bluetoothServer; // Servidor Bluetooth RFCOMM
    BleServer* m_bleServer;           // Servidor Bluetooth LE

    // Flag de suporte a BLE (Bluetooth Low Energy)
    bool m_isBleSupported;
};

#endif