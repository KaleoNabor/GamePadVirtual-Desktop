#include "wifi_server.h"
#include <QNetworkDatagram>
#include <QDebug>
#include <QHostInfo>

// Mensagens para o protocolo de descoberta
const QByteArray DISCOVERY_QUERY = "DISCOVER_GAMEPAD_VIRTUAL_SERVER";
const QByteArray DISCOVERY_ACK_PREFIX = "GAMEPAD_VIRTUAL_SERVER_ACK:";

WifiServer::WifiServer(QObject* parent) : QObject(parent), m_udpSocket(nullptr) {}

void WifiServer::startListening(quint16 port)
{
    m_udpSocket = new QUdpSocket(this);
    connect(m_udpSocket, &QUdpSocket::readyRead, this, &WifiServer::onReadyRead);

    if (m_udpSocket->bind(QHostAddress::AnyIPv4, port, QUdpSocket::ShareAddress)) {
        qDebug() << "Servidor de Descoberta (UDP) ouvindo na porta" << port;
    }
    else {
        qDebug() << "Erro: Nao foi possivel iniciar o servidor de descoberta.";
    }
}

void WifiServer::stopListening()
{
    if (m_udpSocket) {
        m_udpSocket->close();
    }
}

void WifiServer::onReadyRead()
{
    while (m_udpSocket->hasPendingDatagrams())
    {
        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();
        QByteArray data = datagram.data();

        // Verifica se a mensagem recebida é a nossa query de descoberta
        if (data == DISCOVERY_QUERY) {
            qDebug() << "Query de descoberta recebida de" << datagram.senderAddress().toString();

            // Prepara e envia a resposta
            QByteArray response = DISCOVERY_ACK_PREFIX + QHostInfo::localHostName().toUtf8();
            m_udpSocket->writeDatagram(response, datagram.senderAddress(), datagram.senderPort());
        }
    }
}