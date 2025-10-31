#ifndef BLUETOOTH_SERVER_H
#define BLUETOOTH_SERVER_H

#include <QObject>
#include <QBluetoothServer>
#include <QBluetoothSocket>
#include <QList>
#include "../protocol/gamepad_packet.h"

// Define o n�mero m�ximo de jogadores conectados simultaneamente
#define MAX_PLAYERS 8

// Classe principal do servidor Bluetooth para gerenciar conex�es de jogadores
class BluetoothServer : public QObject
{
    Q_OBJECT

public:
    explicit BluetoothServer(QObject* parent = nullptr);
    ~BluetoothServer();

    // --- SE��O: M�TODOS P�BLICOS ---

    // Fun��o para envio de dados para jogador espec�fico
    bool sendToPlayer(int playerIndex, const QByteArray& data);

public slots:
    // --- SE��O: SLOTS P�BLICOS ---

    // In�cio e parada do servidor Bluetooth
    void startServer();
    void stopServer();

    // --- MODIFICA��O ADICIONADA (REQ 2 FIX) ---
    // For�a a desconex�o de um jogador espec�fico via Bluetooth
    void forceDisconnectPlayer(int playerIndex);

private slots:
    // --- SE��O: SLOTS PRIVADOS ---

    // Slots para eventos de conex�o Bluetooth
    void clientConnected();      // Gerencia novas conex�es de clientes
    void readSocket();           // Processa dados recebidos dos sockets
    void clientDisconnected();   // Trata desconex�es de clientes

signals:
    // --- SE��O: SINAIS ---

    // Sinais para comunica��o externa
    void packetReceived(int playerIndex, const GamepadPacket& packet);  // Pacote de gamepad recebido
    void playerConnected(int playerIndex, const QString& type);         // Novo jogador conectado
    void playerDisconnected(int playerIndex);                           // Jogador desconectado
    void logMessage(const QString& message);                            // Mensagens de log

private:
    // --- SE��O: M�TODOS PRIVADOS ---

    // Busca de slot vazio para jogador
    int findEmptySlot() const;

    // --- SE��O: MEMBROS PRIVADOS ---

    // Servidor Bluetooth principal (RFCOMM)
    QBluetoothServer* m_btServer;
    // Lista de sockets de clientes conectados
    QList<QBluetoothSocket*> m_clientSockets;
    // Mapeamento de sockets para �ndices de jogador
    QHash<QBluetoothSocket*, int> m_socketPlayerMap;
    // Array de slots de jogador ocupados (controle de disponibilidade)
    bool m_playerSlots[MAX_PLAYERS];
};

#endif