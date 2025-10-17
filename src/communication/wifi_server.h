#ifndef WIFI_SERVER_H
#define WIFI_SERVER_H

#include <QObject>
#include <QUdpSocket>

class WifiServer : public QObject
{
    Q_OBJECT

public:
    explicit WifiServer(QObject* parent = nullptr);

public slots:
    // In�cio da escuta na porta especificada
    void startListening(quint16 port);
    // Parada da escuta
    void stopListening();

private slots:
    // Slot para leitura de dados recebidos
    void onReadyRead();

private:
    // Socket UDP para comunica��o de descoberta
    QUdpSocket* m_udpSocket;
};

#endif