#ifndef BLE_SERVER_H
#define BLE_SERVER_H

#include <QObject>
#include <QtBluetooth/QLowEnergyController>
#include <QtBluetooth/QLowEnergyService>
#include <QtBluetooth/QLowEnergyCharacteristic>
#include <QtBluetooth/QBluetoothAddress>
#include <QMutex> // CORREÇÃO: Adicionado para thread-safety
#include <QHash>
#include <QTimer>

// Classe principal do servidor BLE (Bluetooth Low Energy) para gerenciar conexões de jogadores
class BleServer : public QObject
{
    Q_OBJECT

public:
    explicit BleServer(QObject* parent = nullptr);
    ~BleServer();

    // --- SEÇÃO: MÉTODOS PÚBLICOS ---

    // Envio de comando de vibração para jogador
    bool sendVibration(int playerIndex, const QByteArray& command);

public slots:
    // --- SEÇÃO: SLOTS PÚBLICOS ---

    // Início e parada do servidor BLE
    void startServer();
    void stopServer();

    // Força a desconexão de um jogador específico via BLE
    void forceDisconnectPlayer(int playerIndex);

private slots:
    // --- SEÇÃO: SLOTS PRIVADOS ---

    // Slots para eventos de conexão BLE
    void onClientConnected();      // Gerencia novas conexões de clientes
    void onClientDisconnected();   // Trata desconexões de clientes
    void onCharacteristicWritten(const QLowEnergyCharacteristic& characteristic, const QByteArray& newValue); // Processa dados recebidos

signals:
    // --- SEÇÃO: SINAIS ---

    // Sinais para comunicação externa
    void packetReceived(int playerIndex, const QByteArray& packetData);  // Dados recebidos do gamepad
    void playerConnected(int playerIndex, const QString& type);          // Novo jogador conectado
    void playerDisconnected(int playerIndex);                            // Jogador desconectado
    void logMessage(const QString& message);                             // Mensagens de log

private:
    // --- SEÇÃO: MÉTODOS PRIVADOS ---

    // Configuração do serviço BLE e características
    void setupService();
    // Busca de slot vazio para novo jogador
    int findEmptySlot() const;

    // --- SEÇÃO: MEMBROS PRIVADOS ---

    // Controlador BLE principal (GATT Server)
    QLowEnergyController* m_bleController = nullptr;
    // Serviço do gamepad (contém características de entrada/saída)
    QLowEnergyService* m_gamepadService = nullptr;
    // Característica de vibração (para feedback háptico)
    QLowEnergyCharacteristic m_vibrationCharacteristic;

    // Estruturas protegidas por mutex
    // Mapeamento de endereços de clientes para índices de jogador
    QHash<QBluetoothAddress, int> m_clientPlayerMap;
    // Array de slots de jogador ocupados (8 jogadores máximo)
    bool m_playerSlots[8];

    // CORREÇÃO: Mutex para proteção de acesso concorrente
    mutable QMutex m_mutex;
};

#endif