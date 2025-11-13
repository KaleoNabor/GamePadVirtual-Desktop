#ifndef UDP_SERVER_H
#define UDP_SERVER_H

#include <QObject>
#include <QUdpSocket>
#include <QHostAddress>
#include <QHash>
#include <QTimer>
#include "../controller_types.h"
#include "../protocol/gamepad_packet.h"


// Classe principal do servidor UDP para gerenciar conexões de jogadores
class UdpServer : public QObject
{
    Q_OBJECT

public:
    explicit UdpServer(QObject* parent = nullptr);
    ~UdpServer();

    // Envia dados para um jogador específico
    bool sendToPlayer(int playerIndex, const QByteArray& data);

public slots:
    // --- SEÇÃO: SLOTS PÚBLICOS ---

    // Inicia o servidor na porta especificada
    void startServer(quint16 port);
    // Para o servidor e limpa recursos
    void stopServer();

    // Força a desconexão de um jogador específico
    void forceDisconnectPlayer(int playerIndex);

private slots:
    // --- SEÇÃO: SLOTS PRIVADOS ---

    // Processa datagramas recebidos dos clientes
    void processPendingDatagrams();

signals:
    // --- SEÇÃO: SINAIS ---

    // Sinaliza recebimento de pacote de gamepad
    void packetReceived(int playerIndex, const GamepadPacket& packet);
    // Notifica conexão de novo jogador
    void playerConnected(int playerIndex, const QString& type);
    // Notifica desconexão de jogador
    void playerDisconnected(int playerIndex);
    // Envia mensagens de log
    void logMessage(const QString& message);

private:
    // --- SEÇÃO: ESTRUTURAS DE DADOS PRIVADAS ---

    // Identificação única do cliente (endereço + porta)
    struct ClientId {
        QHostAddress address;
        quint16 port;
        bool operator==(const ClientId& other) const {
            return address == other.address && port == other.port;
        }
    };

    // Função hash para ClientId (necessária para QHash)
    friend uint qHash(const ClientId& key, uint seed) {
        return qHash(key.address.toString(), seed) ^ key.port;
    }

    // --- SEÇÃO: MÉTODOS PRIVADOS ---

    // Encontra slot vazio para novo jogador
    int findEmptySlot() const;
    // Processa desconexão de jogador
    void handlePlayerDisconnect(int playerIndex);

    // --- SEÇÃO: MEMBROS PRIVADOS ---

    // Socket UDP para comunicação com os clientes
    QUdpSocket* m_udpSocket;

    // CORREÇÃO: m_processingTimer removido - não é necessário
    // pois usamos o sinal readyRead do QUdpSocket

    // Mapeamento cliente -> índice do jogador
    QHash<ClientId, int> m_clientPlayerMap;
    // Mapeamento índice do jogador -> cliente
    QHash<int, ClientId> m_playerClientMap;
    // Controle de slots ocupados (array booleano)
    bool m_playerSlots[MAX_PLAYERS];
};

#endif