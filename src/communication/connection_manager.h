#ifndef CONNECTION_MANAGER_H
#define CONNECTION_MANAGER_H

#include <QObject>
#include <QThread>
#include "wifi_server.h"
#include "udp_server.h"
#include "bluetooth_server.h"
#include "ble_server.h"
#include "../virtual_gamepad/gamepad_manager.h"

// Classe principal para gerenciar todos os serviços de conexão
class ConnectionManager : public QObject
{
    Q_OBJECT

public:
    explicit ConnectionManager(GamepadManager* gamepadManager, QObject* parent = nullptr);
    ~ConnectionManager();

public slots:
    // --- SEÇÃO: SLOTS PÚBLICOS ---

    // Início de todos os serviços de conexão
    void startServices();
    // Parada de todos os serviços de conexão
    void stopServices();

    // --- MODIFICAÇÃO ADICIONADA (REQ 2 FIX) ---
    // Força a desconexão de um jogador específico por tipo de conexão
    void forceDisconnectPlayer(int playerIndex, const QString& type);

signals:
    // --- SEÇÃO: SINAIS ---

    // Sinal para mensagens de log
    void logMessage(const QString& message);

private:
    // --- SEÇÃO: MEMBROS PRIVADOS ---

    // Ponteiro para o gerenciador de gamepads
    GamepadManager* m_gamepadManager;

    // Threads para cada tipo de serviço (execução em paralelo)
    QThread m_discoveryThread;    // Thread para servidor de descoberta Wi-Fi
    QThread m_udpDataThread;      // Thread para servidor de dados UDP
    QThread m_bluetoothThread;    // Thread para servidor Bluetooth clássico
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