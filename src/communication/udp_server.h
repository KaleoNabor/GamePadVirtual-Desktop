#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QHash>
#include <QTimer>
#include "../protocol/gamepad_packet.h"

#define MAX_PLAYERS 8

// Classe principal do servidor UDP para gerenciar conex�es de jogadores
class UdpServer : public QObject
{
    Q_OBJECT

public:
    explicit UdpServer(QObject* parent = nullptr);
    ~UdpServer();

    // Envia dados para um jogador espec�fico
    bool sendToPlayer(int playerIndex, const QByteArray& data);

public slots:
    // --- SE��O: SLOTS P�BLICOS ---

    // Inicia o servidor na porta especificada
    void startServer(quint16 port);
    // Para o servidor e limpa recursos
    void stopServer();

    // --- MODIFICA��O ADICIONADA (REQ 2 FIX) ---
    // For�a a desconex�o de um jogador espec�fico
    void forceDisconnectPlayer(int playerIndex);

private slots:
    // --- SE��O: SLOTS PRIVADOS ---

    // Processa datagramas recebidos dos clientes
    void processPendingDatagrams();

signals:
    // --- SE��O: SINAIS ---

    // Sinaliza recebimento de pacote de gamepad
    void packetReceived(int playerIndex, const GamepadPacket& packet);
    // Notifica conex�o de novo jogador
    void playerConnected(int playerIndex, const QString& type);
    // Notifica desconex�o de jogador
    void playerDisconnected(int playerIndex);
    // Envia mensagens de log
    void logMessage(const QString& message);

private:
    // --- SE��O: ESTRUTURAS DE DADOS PRIVADAS ---

    // Identifica��o �nica do cliente (endere�o + porta)
    struct ClientId {
        QHostAddress address;
        quint16 port;
        bool operator==(const ClientId& other) const {
            return address == other.address && port == other.port;
        }
    };

    // Fun��o hash para ClientId (necess�ria para QHash)
    friend uint qHash(const ClientId& key, uint seed) {
        return qHash(key.address.toString(), seed) ^ key.port;
    }

    // --- SE��O: M�TODOS PRIVADOS ---

    // Encontra slot vazio para novo jogador
    int findEmptySlot() const;
    // Processa desconex�o de jogador
    void handlePlayerDisconnect(int playerIndex);

    // --- SE��O: MEMBROS PRIVADOS ---

    // Socket UDP para comunica��o com os clientes
    QUdpSocket* m_udpSocket;
    // Timer para processamento em tempo real
    QTimer* m_processingTimer;
    // Mapeamento cliente -> �ndice do jogador
    QHash<ClientId, int> m_clientPlayerMap;
    // Mapeamento �ndice do jogador -> cliente
    QHash<int, ClientId> m_playerClientMap;
    // Controle de slots ocupados (array booleano)
    bool m_playerSlots[MAX_PLAYERS];
};

#endif