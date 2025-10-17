#include "wifi_server.h"
#include <QNetworkDatagram>
#include <QDebug>
#include <QHostInfo>

// Mensagens do protocolo de descoberta automática
const QByteArray DISCOVERY_QUERY = "DISCOVER_GAMEPAD_VIRTUAL_SERVER";
const QByteArray DISCOVERY_ACK_PREFIX = "GAMEPAD_VIRTUAL_SERVER_ACK:";

WifiServer::WifiServer(QObject* parent) : QObject(parent), m_udpSocket(nullptr) {}

void WifiServer::startListening(quint16 port)
{
    // Criação e configuração do socket UDP
    m_udpSocket = new QUdpSocket(this);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &WifiServer::onReadyRead);

    // Bind do socket para escuta em qualquer endereço IPv4
    if (m_udpSocket->bind(QHostAddress::AnyIPv4, port, QUdpSocket::ShareAddress)) {
        qDebug() << "Servidor de Descoberta (UDP) ouvindo na porta" << port;
    }
    else {
        qDebug() << "Erro: Nao foi possivel iniciar o servidor de descoberta.";
    }
}

void WifiServer::stopListening()
{
    // Parada do servidor de descoberta
    if (m_udpSocket) {
        m_udpSocket->close();
    }
}

void WifiServer::onReadyRead()
{
    // Processamento de datagramas recebidos
    while (m_udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        QByteArray data = datagram.data();

        // Verificação de query de descoberta
        if (data == DISCOVERY_QUERY) {
            qDebug() << "Query de descoberta recebida de" << datagram.senderAddress().toString();

            // Preparação e envio da resposta de acknowledge
            QByteArray response = DISCOVERY_ACK_PREFIX + QHostInfo::localHostName().toUtf8();
            m_udpSocket->writeDatagram(response, datagram.senderAddress(), datagram.senderPort());
        }
    }
}