#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QHash>
#include <QTimer>
#include "../protocol/gamepad_packet.h"

#define MAX_PLAYERS 4

class UdpServer : public QObject
{
    Q_OBJECT

public:
    explicit UdpServer(QObject* parent = nullptr);
    ~UdpServer();

    // Envia dados para um jogador espec�fico
    bool sendToPlayer(int playerIndex, const QByteArray& data);

public slots:
    // Inicia o servidor na porta especificada
    void startServer(quint16 port);
    // Para o servidor e limpa recursos
    void stopServer();

private slots:
    // Processa datagramas recebidos
    void processPendingDatagrams();

signals:
    // Sinaliza recebimento de pacote de gamepad
    void packetReceived(int playerIndex, const GamepadPacket& packet);
    // Notifica conex�o de novo jogador
    void playerConnected(int playerIndex, const QString& type);
    // Notifica desconex�o de jogador
    void playerDisconnected(int playerIndex);
    // Envia mensagens de log
    void logMessage(const QString& message);

private:
    // Identifica��o �nica do cliente
    struct ClientId {
        QHostAddress address;
        quint16 port;
        bool operator==(const ClientId& other) const {
            return address == other.address && port == other.port;
        }
    };
    friend uint qHash(const ClientId& key, uint seed) {
        return qHash(key.address.toString(), seed) ^ key.port;
    }

    // Encontra slot vazio para novo jogador
    int findEmptySlot() const;
    // Processa desconex�o de jogador
    void handlePlayerDisconnect(int playerIndex);

    // Socket UDP para comunica��o
    QUdpSocket* m_udpSocket;
    // Timer para processamento em tempo real
    QTimer* m_processingTimer;
    // Mapeamento cliente -> �ndice do jogador
    QHash<ClientId, int> m_clientPlayerMap;
    // Mapeamento �ndice do jogador -> cliente
    QHash<int, ClientId> m_playerClientMap;
    // Controle de slots ocupados
    bool m_playerSlots[MAX_PLAYERS];
};

#endif