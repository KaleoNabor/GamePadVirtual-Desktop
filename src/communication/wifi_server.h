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
    void startListening(quint16 port);
    void stopListening();

private slots:
    void onReadyRead();

private:
    QUdpSocket* m_udpSocket;
};

#endif // WIFI_SERVER_H