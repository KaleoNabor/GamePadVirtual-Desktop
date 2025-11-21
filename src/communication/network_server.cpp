#include "network_server.h"

#include <QNetworkDatagram>

#include <QHostInfo>

#include "../utils/input_emulator.h"



NetworkServer::NetworkServer(QObject* parent)

    : QObject(parent),

    m_tcpServer(nullptr),

    m_udpSocket(nullptr),

    m_discoverySocket(nullptr),

    m_streamer(nullptr),

    m_lastLeftClick(false),

    m_lastRightClick(false)

{

    for (int i = 0; i < MAX_PLAYERS; ++i) {

        m_playerSlots[i] = false;

    }



    m_streamer = new ScreenStreamer(this);

    // REMOVA: m_streamer->startMasterPipeline();  <-- NÃO INICIA MAIS AUTOMÁTICO



    // Conecta mudança de estado do Streamer para avisar a UI (e clientes se quiser)

    connect(m_streamer, &ScreenStreamer::streamStateChanged, this, [](bool active) {

        qDebug() << "📡 Status do Stream mudou para:" << active;

        });



    // Conecta o sinal roteado

    connect(m_streamer, &ScreenStreamer::sendSignalingMessage, this,

        [this](int playerIndex, const QJsonObject& json) {

            qDebug() << "📨 [Sinalização] Enviando mensagem para Player" << playerIndex << "Tipo:" << json["type"].toString();



            QJsonDocument doc(json);

            QByteArray data = "JSON:" + doc.toJson(QJsonDocument::Compact);



            // Busca o socket específico deste jogador

            QTcpSocket* targetSocket = nullptr;

            for (auto it = m_socketPlayerMap.begin(); it != m_socketPlayerMap.end(); ++it) {

                if (it.value() == playerIndex) {

                    targetSocket = it.key();

                    break;

                }

            }



            if (targetSocket) {

                qDebug() << "📤 [Sinalização] Enviando" << data.length() << "bytes para Player" << playerIndex;

                targetSocket->write(data);

                bool flushed = targetSocket->flush();

                qDebug() << (flushed ? "✅" : "❌") << "[Sinalização] Dados" << (flushed ? "enviados" : "NÃO enviados") << "para Player" << playerIndex;

            }

            else {

                qWarning() << "⚠️ [Sinalização] Socket não encontrado para Player" << playerIndex;

            }

        });

}



NetworkServer::~NetworkServer()

{

    stopServer();

}



// Método público para ligar/desligar (chamado pela UI)

void NetworkServer::setStreamingEnabled(bool enabled) {

    m_streamer->setStreamingEnabled(enabled);

}



bool NetworkServer::isStreamingEnabled() const {

    return m_streamer->isStreamingEnabled();

}



void NetworkServer::startServer()

{

    qDebug() << "🚀 Iniciando servidor de rede...";



    // Servidor de controle TCP

    m_tcpServer = new QTcpServer(this);

    connect(m_tcpServer, &QTcpServer::newConnection, this, &NetworkServer::newTcpConnection);



    if (!m_tcpServer->listen(QHostAddress::Any, CONTROL_PORT_TCP)) {

        qCritical() << "❌ Falha ao iniciar servidor TCP na porta" << CONTROL_PORT_TCP;

        emit logMessage("Erro: Falha ao iniciar servidor TCP.");

        delete m_tcpServer;

        m_tcpServer = nullptr;

        return;

    }

    qDebug() << "✅ Servidor TCP listening na porta" << CONTROL_PORT_TCP;



    // Servidor de dados UDP

    m_udpSocket = new QUdpSocket(this);

    connect(m_udpSocket, &QUdpSocket::readyRead, this, &NetworkServer::readUdpDatagrams);



    if (!m_udpSocket->bind(QHostAddress::Any, DATA_PORT_UDP)) {

        qCritical() << "❌ Falha ao iniciar servidor UDP na porta" << DATA_PORT_UDP;

        emit logMessage("Erro: Falha ao iniciar servidor UDP.");

        stopServer();

        return;

    }

    qDebug() << "✅ Servidor UDP bound na porta" << DATA_PORT_UDP;



    // Servidor de descoberta UDP

    m_discoverySocket = new QUdpSocket(this);

    connect(m_discoverySocket, &QUdpSocket::readyRead, this, &NetworkServer::readDiscoveryDatagrams);



    if (!m_discoverySocket->bind(QHostAddress::AnyIPv4, DISCOVERY_PORT, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint)) {

        qCritical() << "❌ Falha ao iniciar servidor de Descoberta na porta" << DISCOVERY_PORT;

        emit logMessage("Erro: Falha ao iniciar servidor de Descoberta.");

        stopServer();

        return;

    }

    qDebug() << "✅ Servidor de Descoberta bound na porta" << DISCOVERY_PORT;



    emit logMessage(QString("Servidor de Rede iniciado (TCP: %1, UDP: %2, Descoberta: %3)")

        .arg(CONTROL_PORT_TCP)

        .arg(DATA_PORT_UDP)

        .arg(DISCOVERY_PORT));



    qDebug() << "🎉 Servidor totalmente inicializado e aguardando conexões!";

    qDebug() << "📡 Streaming está:" << (isStreamingEnabled() ? "LIGADO" : "DESLIGADO");

}



void NetworkServer::stopServer()

{

    qDebug() << "🛑 Parando servidor de rede...";



    if (m_tcpServer) {

        m_tcpServer->close();

        delete m_tcpServer;

        m_tcpServer = nullptr;

        qDebug() << "✅ Servidor TCP parado";

    }

    if (m_udpSocket) {

        m_udpSocket->close();

        delete m_udpSocket;

        m_udpSocket = nullptr;

        qDebug() << "✅ Servidor UDP parado";

    }

    if (m_discoverySocket) {

        m_discoverySocket->close();

        delete m_discoverySocket;

        m_discoverySocket = nullptr;

        qDebug() << "✅ Servidor de Descoberta parado";

    }



    qDeleteAll(m_socketPlayerMap.keys());

    m_socketPlayerMap.clear();

    m_ipPlayerMap.clear();

    m_playerIpMap.clear();

    m_playerUdpPortMap.clear();



    for (int i = 0; i < MAX_PLAYERS; ++i) {

        m_playerSlots[i] = false;

    }

    qDebug() << "✅ Servidor de Rede totalmente parado.";

}



int NetworkServer::findEmptySlot() const

{

    for (int i = 0; i < MAX_PLAYERS; ++i) {

        if (!m_playerSlots[i]) {

            return i;

        }

    }

    return -1;

}



// Canal de controle TCP

void NetworkServer::newTcpConnection()

{

    QTcpSocket* socket = m_tcpServer->nextPendingConnection();

    if (!socket) {

        qWarning() << "⚠️ Socket de conexão pendente é nulo!";

        return;

    }



    socket->setParent(this);



    QHostAddress clientAddress = socket->peerAddress();

    if (clientAddress.protocol() == QAbstractSocket::IPv6Protocol) {

        clientAddress = QHostAddress(clientAddress.toIPv4Address());

    }



    qDebug() << "🔗 Nova conexão TCP de" << clientAddress.toString() << ":" << socket->peerPort();



    int playerIndex = findEmptySlot();

    if (playerIndex == -1) {

        qWarning() << "❌ Servidor cheio! Rejeitando conexão de" << clientAddress.toString();

        socket->close();

        socket->deleteLater();

        return;

    }



    if (m_ipPlayerMap.contains(clientAddress)) {

        qWarning() << "⚠️ IP" << clientAddress.toString() << "já está conectado. Rejeitando duplicata.";

        socket->close();

        socket->deleteLater();

        return;

    }



    connect(socket, &QTcpSocket::disconnected, this, &NetworkServer::tcpClientDisconnected);

    connect(socket, &QTcpSocket::readyRead, this, &NetworkServer::readTcpSocket);

    connect(socket, &QTcpSocket::errorOccurred, this, &NetworkServer::tcpSocketError);



    m_playerSlots[playerIndex] = true;

    m_socketPlayerMap[socket] = playerIndex;

    m_ipPlayerMap[clientAddress] = playerIndex;

    m_playerIpMap[playerIndex] = clientAddress;



    qDebug() << "👤 Novo jogador" << (playerIndex + 1) << "conectado via TCP/IP:" << clientAddress.toString();

    emit playerConnected(playerIndex, "Wi-Fi");



    // Informa o estado atual do streaming para o novo cliente

    if (!isStreamingEnabled()) {

        qDebug() << "ℹ️ Streaming está DESLIGADO para novo cliente" << playerIndex;

    }



    // Log do estado atual dos slots

    qDebug() << "📊 Slots ocupados:" << m_socketPlayerMap.size() << "/" << MAX_PLAYERS;

}



void NetworkServer::tcpClientDisconnected()

{

    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());

    if (!socket || !m_socketPlayerMap.contains(socket)) {

        qWarning() << "⚠️ Socket desconectado não encontrado no mapa!";

        return;

    }



    int playerIndex = m_socketPlayerMap[socket];

    QHostAddress clientAddress = socket->peerAddress();

    if (clientAddress.protocol() == QAbstractSocket::IPv6Protocol) {

        clientAddress = QHostAddress(clientAddress.toIPv4Address());

    }



    qDebug() << "🔌 Jogador" << (playerIndex + 1) << "desconectado (TCP/IP):" << clientAddress.toString();



    m_playerSlots[playerIndex] = false;

    m_socketPlayerMap.remove(socket);

    m_ipPlayerMap.remove(clientAddress);

    m_playerIpMap.remove(playerIndex);

    m_playerUdpPortMap.remove(playerIndex);



    // Limpa o ramo do GStreamer para economizar RAM

    qDebug() << "🗑️ Removendo cliente do ScreenStreamer...";

    m_streamer->removeClient(playerIndex);



    socket->deleteLater();



    qDebug() << "✅ Jogador" << (playerIndex + 1) << "totalmente removido.";

    emit playerDisconnected(playerIndex);



    // Log do estado atual

    qDebug() << "📊 Slots restantes:" << m_socketPlayerMap.size() << "/" << MAX_PLAYERS;

}



void NetworkServer::readTcpSocket()

{

    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());

    if (!socket) {

        qWarning() << "⚠️ Socket de leitura é nulo!";

        return;

    }



    int playerIndex = m_socketPlayerMap.value(socket, -1);

    if (playerIndex == -1) {

        qWarning() << "⚠️ Socket não mapeado para nenhum player!";

        return;

    }



    QByteArray data = socket->readAll();

    qDebug() << "📨 [TCP] Dados recebidos do Player" << playerIndex << "Tamanho:" << data.size() << "bytes";



    if (data.startsWith("JSON:")) {

        QByteArray jsonData = data.mid(5);

        qDebug() << "📨 [TCP] JSON do Player" << playerIndex << "Tamanho:" << jsonData.size() << "bytes";



        QJsonDocument doc = QJsonDocument::fromJson(jsonData);

        if (doc.isNull()) {

            qWarning() << "❌ [TCP] JSON inválido do Player" << playerIndex;

            return;

        }



        QJsonObject obj = doc.object();

        QString type = obj["type"].toString();



        qDebug() << "📨 [TCP] Processando mensagem do Player" << playerIndex << "Tipo:" << type;



        if (obj["type"] == "request_stream") {

            if (isStreamingEnabled()) {

                qDebug() << "🎬 [TCP] Player" << playerIndex << "solicitou stream - adicionando cliente...";

                m_streamer->addClient(playerIndex);

            }

            else {

                // Opcional: Enviar mensagem de erro "Stream desligado pelo host"

                qDebug() << "⚠️ Cliente" << playerIndex << "pediu stream, mas está DESLIGADO.";



                // Envia mensagem de erro para o cliente

                QJsonObject errorMsg;

                errorMsg["type"] = "stream_error";

                errorMsg["message"] = "Streaming está desligado pelo host";



                QJsonDocument errorDoc(errorMsg);

                QByteArray errorData = "JSON:" + errorDoc.toJson(QJsonDocument::Compact);

                socket->write(errorData);

                socket->flush();

            }

        }

        // --- NOVO COMANDO ---

        else if (obj["type"] == "toggle_stream_master") {

            bool enabled = obj["enabled"].toBool();

            setStreamingEnabled(enabled);

            qDebug() << "📱 Comando remoto recebido: Stream" << (enabled ? "LIGADO" : "DESLIGADO");

        }

        // --------------------

        else {

            // Resposta SDP ou Candidate de um cliente específico

            qDebug() << "📡 [TCP] Sinalização recebida do Player" << playerIndex << ":" << type;

            m_streamer->handleSignalingMessage(playerIndex, obj);

        }

    }

    else {

        // Lógica antiga (Keep Alive 0x01, etc)

        qDebug() << "📨 [TCP] Dados não-JSON do Player" << playerIndex << "Primeiros bytes:" << data.left(10).toHex();

        // Processa outros tipos de dados TCP aqui se necessário

    }

}



// CORREÇÃO: Implementação da função de erro

void NetworkServer::tcpSocketError(QAbstractSocket::SocketError socketError)

{

    QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());

    if (!socket) return;



    int playerIndex = m_socketPlayerMap.value(socket, -1);

    qWarning() << "❌ [TCP] Erro de Socket (Jogador" << playerIndex << "):" << socket->errorString();

    // A desconexão será tratada pelo slot tcpClientDisconnected, que é chamado automaticamente.

}



// Canal de dados UDP

void NetworkServer::readUdpDatagrams()

{

    while (m_udpSocket->hasPendingDatagrams()) {

        QNetworkDatagram datagram = m_udpSocket->receiveDatagram();

        QByteArray data = datagram.data();



        QHostAddress senderAddress = datagram.senderAddress();

        if (senderAddress.protocol() == QAbstractSocket::IPv6Protocol) {

            senderAddress = QHostAddress(senderAddress.toIPv4Address());

        }



        quint16 senderPort = datagram.senderPort();



        //qDebug() << "📨 [UDP] Datagrama recebido de" << senderAddress.toString() << ":" << senderPort

            //<< "Tamanho:" << data.size() << "bytes";



        if (!m_ipPlayerMap.contains(senderAddress)) {

            //qDebug() << "⚠️ [UDP] IP não registrado:" << senderAddress.toString() << "- ignorando datagrama";

            continue;

        }



        int playerIndex = m_ipPlayerMap[senderAddress];



        if (!m_playerUdpPortMap.contains(playerIndex)) {

            m_playerUdpPortMap.insert(playerIndex, senderPort);

            qDebug() << "📝 [UDP] Jogador" << (playerIndex + 1) << "registrou porta UDP:" << senderPort;

        }



        // --- LÓGICA DE DISCRIMINAÇÃO DE PACOTE ---



        // 1. Pacote de MOUSE (6 bytes)

        if (data.size() == 6 && (quint8)data[0] == 0x02) {

            const char* ptr = data.constData();

            int16_t dx = *reinterpret_cast<const int16_t*>(ptr + 1);
            int16_t dy = *reinterpret_cast<const int16_t*>(ptr + 3);
            uint8_t btns = (uint8_t)ptr[5];

            // 1. Movimento
            if (dx != 0 || dy != 0) {
                InputEmulator::moveMouse(dx, dy);
            }

            // 2. Cliques (Com verificação de estado para não travar o Windows)
            bool currentLeft = (btns & 1);
            bool currentRight = (btns & 2);

            // Lógica do Botão Esquerdo
            if (currentLeft != m_lastLeftClick) {
                // Estado mudou
                InputEmulator::mouseClick(true, currentLeft); // true=left, currentLeft defines DOWN(true) or UP(false)
                m_lastLeftClick = currentLeft;
            }

            // Lógica do Botão Direito
            if (currentRight != m_lastRightClick) {
                InputEmulator::mouseClick(false, currentRight); // false=right
                m_lastRightClick = currentRight;
            }
        }



        // 2. Pacote de GAMEPAD (Tamanho fixo 20 bytes)

        else if (data.size() == sizeof(GamepadPacket)) {

            //qDebug() << "🎮 [UDP] Pacote de gamepad do Player" << playerIndex;

            const GamepadPacket* packet = reinterpret_cast<const GamepadPacket*>(data.constData());

            emit packetReceived(playerIndex, *packet);

        }



        // 3. Pacote não reconhecido

        else {

            qDebug() << "⚠️ [UDP] Pacote não reconhecido de" << senderAddress.toString()

                << "tamanho:" << data.size() << "bytes";

            qDebug() << "   - Primeiros bytes:" << data.left(10).toHex();

        }

    }

}



// Canal de descoberta UDP

void NetworkServer::readDiscoveryDatagrams()

{

    while (m_discoverySocket->hasPendingDatagrams()) {

        QNetworkDatagram datagram = m_discoverySocket->receiveDatagram();

        QByteArray data = datagram.data();



        QHostAddress senderAddress = datagram.senderAddress();

        quint16 senderPort = datagram.senderPort();



        qDebug() << "🔍 [Descoberta] Datagrama recebido de" << senderAddress.toString()

            << "Tamanho:" << data.size() << "bytes"

            << "Conteúdo:" << data;



        if (data == "DISCOVER_GAMEPAD_VIRTUAL_SERVER") {

            qDebug() << "🎯 [Descoberta] Pedido de descoberta recebido de" << senderAddress.toString();



            QString hostName = QHostInfo::localHostName();

            if (hostName.isEmpty()) {

                hostName = "Servidor-PC";

            }



            QByteArray response = "GAMEPAD_VIRTUAL_SERVER_ACK:" + hostName.toUtf8();



            qDebug() << "📤 [Descoberta] Enviando resposta para" << senderAddress.toString() << ":" << senderPort;

            qDebug() << "   - Resposta:" << response;



            qint64 bytesSent = m_discoverySocket->writeDatagram(response, senderAddress, senderPort);

            if (bytesSent == -1) {

                qWarning() << "❌ [Descoberta] Falha ao enviar resposta de descoberta";

            }

            else {

                qDebug() << "✅ [Descoberta] Resposta enviada -" << bytesSent << "bytes";

            }



        }

        else {

            qDebug() << "⚠️ [Descoberta] Mensagem de descoberta desconhecida:" << data;

        }

    }

}



void NetworkServer::forceDisconnectPlayer(int playerIndex)

{

    qDebug() << "🔌 Forçando desconexão do Player" << playerIndex;



    QTcpSocket* targetSocket = nullptr;

    for (auto it = m_socketPlayerMap.constBegin(); it != m_socketPlayerMap.constEnd(); ++it) {

        if (it.value() == playerIndex) {

            targetSocket = it.key();

            break;

        }

    }

    if (targetSocket) {

        qDebug() << "📡 Fechando socket do Player" << playerIndex;

        targetSocket->close();

    }

    else {

        qWarning() << "⚠️ Socket não encontrado para Player" << playerIndex;

    }

}



void NetworkServer::sendVibration(int playerIndex, const QByteArray& command)

{

    qDebug() << "📳 Enviando vibração para Player" << playerIndex << "Tamanho:" << command.size() << "bytes";



    if (m_playerIpMap.contains(playerIndex) && m_playerUdpPortMap.contains(playerIndex)) {

        QHostAddress address = m_playerIpMap.value(playerIndex);

        quint16 port = m_playerUdpPortMap.value(playerIndex);



        qDebug() << "   - Destino:" << address.toString() << ":" << port;



        qint64 bytesSent = m_udpSocket->writeDatagram(command, address, port);

        if (bytesSent == -1) {

            qWarning() << "❌ Falha ao enviar comando de vibração";

        }

        else {

            qDebug() << "✅ Comando de vibração enviado -" << bytesSent << "bytes";

        }

    }

    else {

        qWarning() << "⚠️ Player" << playerIndex << "não encontrado para envio de vibração";

        qDebug() << "   - IP registrado:" << m_playerIpMap.contains(playerIndex);

        qDebug() << "   - Porta UDP registrada:" << m_playerUdpPortMap.contains(playerIndex);

    }

}