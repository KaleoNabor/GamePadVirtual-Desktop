#ifndef SCREEN_STREAMER_H
#define SCREEN_STREAMER_H

#include <QObject>
#include <QMap>
#include <QMutex>
#include <gst/gst.h>
#include <gst/webrtc/webrtc.h>
#include <QJsonObject>

struct ClientStreamContext {
    int playerId;
    GstElement* webrtcbin = nullptr;
    GstElement* rtp_queue = nullptr;
    //Video
    GstPad* tee_pad = nullptr;
    // Áudio
    GstElement* audio_queue = nullptr;
    GstPad* tee_audio_pad = nullptr;
};

class ScreenStreamer : public QObject
{
    Q_OBJECT

public:
    explicit ScreenStreamer(QObject* parent = nullptr);
    ~ScreenStreamer();

    // Controle Mestre (Botão da UI)
    void setStreamingEnabled(bool enabled);
    bool isStreamingEnabled() const { return m_isStreamingEnabled; }

    // Gerenciamento de Clientes
    void addClient(int playerIndex);
    void removeClient(int playerIndex);
    void handleSignalingMessage(int playerIndex, const QJsonObject& json);

signals:
    void sendSignalingMessage(int playerIndex, const QJsonObject& json);
    void streamError(const QString& errorMsg);
    void streamStateChanged(bool active); // Avisa a UI se o stream caiu ou iniciou

private:
    bool m_isStreamingEnabled = false;

    GstElement* pipeline = nullptr;
    GstElement* tee_video = nullptr;
    GstElement* tee_audio = nullptr;
    GstElement* encoder_element = nullptr;

    QMap<int, ClientStreamContext*> m_clients;
    QMutex m_clientsMutex;

    void startMasterPipeline();
    void stopMasterPipeline();
    void setupMasterPipeline();
    void forceKeyframe();

    // Callbacks GStreamer
    static void onNegotiationNeeded(GstElement* webrtc, gpointer user_data);
    static void onIceCandidate(GstElement* webrtc, guint mline_index, gchar* candidate, gpointer user_data);
    static void onOfferCreated(GstPromise* promise, gpointer user_data);
    static GstBusSyncReply onBusMessage(GstBus* bus, GstMessage* msg, gpointer user_data);
};

struct CallbackData {
    ScreenStreamer* self;
    int playerId;
};

#endif // SCREEN_STREAMER_H