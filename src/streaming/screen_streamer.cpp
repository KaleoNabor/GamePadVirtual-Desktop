#include "screen_streamer.h"
#include <QDebug>
#include <QJsonDocument>
#include <QTimer>
#include <gst/video/video.h>

static void free_callback_data(gpointer data, GClosure* closure) {
    Q_UNUSED(closure);
    if (data) delete static_cast<CallbackData*>(data);
}

// Função de callback para linkar com segurança
static GstPadProbeReturn link_client_pad_cb(GstPad* pad, GstPadProbeInfo* info, gpointer user_data) {
    Q_UNUSED(info);
    ClientStreamContext* ctx = (ClientStreamContext*)user_data;

    // Pega o pad da queue do cliente
    GstPad* queue_pad = gst_element_get_static_pad(ctx->rtp_queue, "sink");

    // Tenta linkar
    if (gst_pad_link(pad, queue_pad) != GST_PAD_LINK_OK) {
        qCritical() << "❌ Erro Crítico: Falha ao linkar Tee -> Queue no Probe!";
    }
    else {
        qDebug() << "✅ Link dinâmico Tee -> Queue realizado com sucesso.";
    }

    gst_object_unref(queue_pad);

    // Remove o probe para deixar os dados fluírem
    return GST_PAD_PROBE_REMOVE;
}

ScreenStreamer::ScreenStreamer(QObject* parent) : QObject(parent)
{
    if (!gst_is_initialized()) gst_init(nullptr, nullptr);
}

ScreenStreamer::~ScreenStreamer()
{
    stopMasterPipeline();
}

void ScreenStreamer::setStreamingEnabled(bool enabled)
{
    if (m_isStreamingEnabled == enabled) return;

    m_isStreamingEnabled = enabled;
    qDebug() << "🎚️ Controle Mestre de Streaming:" << (enabled ? "LIGADO" : "DESLIGADO");

    if (enabled) {
        startMasterPipeline();
    }
    else {
        stopMasterPipeline();
    }
    emit streamStateChanged(enabled);
}

void ScreenStreamer::startMasterPipeline()
{
    if (pipeline) return;
    setupMasterPipeline();
}

void ScreenStreamer::stopMasterPipeline()
{
    if (pipeline) {
        QMutexLocker locker(&m_clientsMutex);
        // Remove clientes antes de matar o mestre
        for (auto ctx : m_clients) delete ctx;
        m_clients.clear();

        gst_element_set_state(pipeline, GST_STATE_NULL);
        gst_object_unref(pipeline);

        pipeline = nullptr;
        tee_video = nullptr;
        tee_audio = nullptr;
        encoder_element = nullptr;
        qDebug() << "🛑 Pipeline Mestre parado e memória liberada.";
    }
}

void ScreenStreamer::setupMasterPipeline()
{
    GError* error = nullptr;

    // --- CONFIGURAÇÃO ULTRA LOW LATENCY (COMPETITIVA) ---

    // 1. Otimização de Rede (MTU)
    // mtu=1200: Evita fragmentação de pacotes em roteadores comuns, reduzindo jitter.
    QString payloader = "rtph264pay config-interval=1 pt=96 aggregate-mode=1 mtu=1200";

    // 2. Configuração do Encoder NVIDIA (Foco: VELOCIDADE)
    // bitrate=8000: 8Mbps é suficiente para celular e muito mais leve para o Wi-Fi.
    // rc-mode=cbr: Taxa constante evita picos de lag.
    // preset=low-latency: Prioriza tempo de resposta sobre qualidade de compressão.
    // gop-size=60: Keyframes regulares para resiliência (1 por segundo a 60fps)
    QString videoEncoder =
        "nvh264enc name=enc preset=low-latency zerolatency=true "
        "bitrate=8000 rc-mode=cbr qp-min=15 qp-max=40 gop-size=60 aud=false";

    // Caps (Mantém compatibilidade Android)
    QString capsVideo = "video/x-h264,profile=baseline,stream-format=byte-stream,alignment=au";

    // 3. Configuração de Áudio (Buffer mínimo = Latência mínima)
    // buffer-time=10000: Reduz buffer de captura para ~10ms (padrão: 200ms)
    QString audioEncoder =
        "opusenc bitrate=96000 frame-size=10 audio-type=restricted-lowdelay inband-fec=true";

    // Ramos do Tee (Fakesink para manter vivo)
    QString masterEnd =
        "tee name=t_vid allow-not-linked=true ! "
        "queue leaky=1 max-size-buffers=1 ! fakesink sync=true async=false";

    QString audioEnd =
        "tee name=t_aud allow-not-linked=true ! "
        "queue leaky=1 max-size-buffers=1 ! fakesink sync=true async=false";

    // --- MONTAGEM ---

    QString videoBranch = QString(
        "d3d11screencapturesrc show-cursor=true ! "
        "video/x-raw(memory:D3D11Memory),framerate=60/1 ! "
        "d3d11convert ! "
        "%1 ! %2 ! " // Encoder + Caps
        "%3 ! "      // Payloader
        "%4").arg(videoEncoder, capsVideo, payloader, masterEnd);

    QString audioBranch = QString(
        "wasapisrc loopback=true buffer-time=10000 ! " // ⚡ buffer-time=10ms (padrão é 200ms)
        "audioconvert ! audioresample ! "
        "%1 ! " // Opus Enc
        "rtpopuspay pt=111 ! "
        "%2").arg(audioEncoder, audioEnd);

    QString fullPipeline = videoBranch + " " + audioBranch;

    qDebug() << "🔧 Inicializando Pipeline ULTRA LOW LATENCY (CBR 8Mbps)...";
    pipeline = gst_parse_launch(fullPipeline.toUtf8().constData(), &error);

    if (error) {
        // Fallback para Software (VP8) se NVENC falhar, mas mantendo Áudio otimizado
        qWarning() << "⚠️ Hardware falhou:" << error->message << ". Usando VP8...";
        g_error_free(error); error = nullptr;

        // Fallback VP8 também otimizado para baixa latência
        QString vp8Branch =
            "d3d11screencapturesrc show-cursor=true ! "
            "video/x-raw(memory:D3D11Memory),framerate=60/1 ! "
            "d3d11download ! queue max-size-buffers=1 ! videoconvert ! "
            "vp8enc deadline=1 cpu-used=16 target-bitrate=8000000 keyframe-max-dist=60 ! " // cpu-used=16 é o mais rápido
            "rtpvp8pay pt=96 ! "
            "tee name=t_vid allow-not-linked=true ! "
            "queue leaky=1 ! fakesink sync=true async=false";

        pipeline = gst_parse_launch((vp8Branch + " " + audioBranch).toUtf8().constData(), &error);
    }

    if (!pipeline) {
        qCritical() << "❌ FALHA TOTAL NO PIPELINE:" << (error ? error->message : "");
        if (error) {
            qCritical() << "🔧 Erro GStreamer: " << error->message;
            g_error_free(error);
        }
        emit streamError("Falha ao iniciar captura de tela e áudio.");
        return;
    }

    tee_video = gst_bin_get_by_name(GST_BIN(pipeline), "t_vid");
    tee_audio = gst_bin_get_by_name(GST_BIN(pipeline), "t_aud");
    encoder_element = gst_bin_get_by_name(GST_BIN(pipeline), "enc"); // Pode ser nulo se for VP8

    if (!tee_video) {
        qCritical() << "❌ Elemento 't_vid' (tee vídeo) não encontrado no pipeline!";
        stopMasterPipeline();
        return;
    }

    if (!tee_audio) {
        qCritical() << "❌ Elemento 't_aud' (tee áudio) não encontrado no pipeline!";
        stopMasterPipeline();
        return;
    }

    // Bus Watcher
    GstBus* bus = gst_element_get_bus(pipeline);
    gst_bus_set_sync_handler(bus, onBusMessage, this, nullptr);
    gst_object_unref(bus);

    gst_element_set_state(pipeline, GST_STATE_PLAYING);
    qDebug() << "✅ Servidor de Streaming A/V RODANDO (Modo Ultra Low Latency).";
}

void ScreenStreamer::addClient(int playerIndex)
{
    // Se o botão mestre estiver desligado, rejeita
    if (!m_isStreamingEnabled) {
        qDebug() << "⚠️ Cliente" << playerIndex << "tentou conectar mas Streaming está DESLIGADO.";
        return;
    }

    if (!pipeline || !tee_video || !tee_audio) {
        startMasterPipeline();
        if (!pipeline) return;
    }

    QMutexLocker locker(&m_clientsMutex);
    if (m_clients.contains(playerIndex)) removeClient(playerIndex);

    qDebug() << "👤 Conectando Cliente A/V:" << playerIndex;
    ClientStreamContext* ctx = new ClientStreamContext();
    ctx->playerId = playerIndex;

    // --- ELEMENTOS DE VÍDEO ---
    ctx->rtp_queue = gst_element_factory_make("queue", NULL); // Fila de vídeo
    g_object_set(ctx->rtp_queue, "leaky", 2, "max-size-buffers", 1, NULL);

    // --- ELEMENTOS DE ÁUDIO (NOVO) ---
    // Guardamos na struct para limpar depois
    ctx->audio_queue = gst_element_factory_make("queue", NULL);
    g_object_set(ctx->audio_queue, "leaky", 2, "max-size-buffers", 5, NULL); // Buffer um pouco maior pra áudio

    // ⚡ WEBRTC COM LATÊNCIA ZERO
    ctx->webrtcbin = gst_element_factory_make("webrtcbin", NULL);
    g_object_set(ctx->webrtcbin,
        "bundle-policy", 3,
        "stun-server", "stun://stun.l.google.com:19302",
        "latency", 0, // ⚡ LATÊNCIA ZERO PARA CLIENTES TAMBÉM
        NULL);

    // Verifica nulidade
    if (!ctx->rtp_queue || !ctx->audio_queue || !ctx->webrtcbin) {
        delete ctx; return;
    }

    gst_bin_add_many(GST_BIN(pipeline), ctx->rtp_queue, ctx->audio_queue, ctx->webrtcbin, NULL);

    // Sync state
    gst_element_sync_state_with_parent(ctx->rtp_queue);
    gst_element_sync_state_with_parent(ctx->audio_queue);
    gst_element_sync_state_with_parent(ctx->webrtcbin);

    // Link Interno (Queue Video -> WebRTC)
    // O webrtcbin cria pads dinamicamente quando linkamos
    if (!gst_element_link_pads(ctx->rtp_queue, "src", ctx->webrtcbin, "sink_%u")) {
        qCritical() << "❌ Erro link Queue Video -> WebRTC";
    }

    // Link Interno (Queue Audio -> WebRTC)
    if (!gst_element_link_pads(ctx->audio_queue, "src", ctx->webrtcbin, "sink_%u")) {
        qCritical() << "❌ Erro link Queue Audio -> WebRTC";
    }

    // --- LINK TEE VÍDEO (Usando Probe para segurança) ---
    ctx->tee_pad = gst_element_request_pad_simple(tee_video, "src_%u");
    gst_pad_add_probe(ctx->tee_pad, GST_PAD_PROBE_TYPE_IDLE, (GstPadProbeCallback)link_client_pad_cb, ctx, NULL);

    // --- LINK TEE ÁUDIO ---
    // Salva o pad na struct
    ctx->tee_audio_pad = gst_element_request_pad_simple(tee_audio, "src_%u");

    GstPad* queue_audio_pad = gst_element_get_static_pad(ctx->audio_queue, "sink");
    if (gst_pad_link(ctx->tee_audio_pad, queue_audio_pad) != GST_PAD_LINK_OK) {
        qCritical() << "❌ Erro Link Tee Audio";
    }
    gst_object_unref(queue_audio_pad);

    // Sinais WebRTC
    CallbackData* cbData = new CallbackData{ this, playerIndex };
    g_signal_connect_data(ctx->webrtcbin, "on-negotiation-needed",
        G_CALLBACK(onNegotiationNeeded), cbData, (GClosureNotify)free_callback_data, (GConnectFlags)0);

    cbData = new CallbackData{ this, playerIndex };
    g_signal_connect_data(ctx->webrtcbin, "on-ice-candidate",
        G_CALLBACK(onIceCandidate), cbData, (GClosureNotify)free_callback_data, (GConnectFlags)0);

    // Transceivers (Vídeo e Áudio)
    GstWebRTCRTPTransceiver* trans = nullptr;

    // Vídeo SendOnly
    g_signal_emit_by_name(ctx->webrtcbin, "add-transceiver", 2, NULL, &trans);
    if (trans) gst_object_unref(trans);

    // Áudio SendOnly
    g_signal_emit_by_name(ctx->webrtcbin, "add-transceiver", 2, NULL, &trans);
    if (trans) gst_object_unref(trans);

    m_clients.insert(playerIndex, ctx);

    // Gatilho de Oferta
    QTimer::singleShot(50, this, [this, playerIndex]() {
        QMutexLocker locker(&m_clientsMutex);
        if (m_clients.contains(playerIndex)) {
            qDebug() << "🎯 Disparando oferta A/V (Delayed)...";
            ClientStreamContext* c = m_clients[playerIndex];

            CallbackData* pData = new CallbackData{ this, playerIndex };
            static auto pFree = [](gpointer data) { delete static_cast<CallbackData*>(data); };
            GstPromise* promise = gst_promise_new_with_change_func(onOfferCreated, pData, pFree);
            g_signal_emit_by_name(c->webrtcbin, "create-offer", nullptr, promise);

            forceKeyframe();
        }
        });
}

void ScreenStreamer::removeClient(int playerIndex)
{
    QMutexLocker locker(&m_clientsMutex);
    if (!m_clients.contains(playerIndex)) return;

    qDebug() << "🗑️ Removendo cliente:" << playerIndex;
    ClientStreamContext* ctx = m_clients.take(playerIndex);

    // 1. Limpeza VÍDEO
    if (ctx->tee_pad && tee_video) { // Use tee_video aqui se renomeou no header, ou tee_element
        GstPad* queue_pad = gst_element_get_static_pad(ctx->rtp_queue, "sink");
        if (queue_pad) {
            gst_pad_unlink(ctx->tee_pad, queue_pad);
            gst_object_unref(queue_pad);
        }
        // Libera o pad do Tee de vídeo
        gst_element_release_request_pad(tee_video, ctx->tee_pad); // ou tee_element
    }

    // 2. Limpeza ÁUDIO (NOVO)
    if (ctx->tee_audio_pad && tee_audio) {
        if (ctx->audio_queue) {
            GstPad* q_aud_pad = gst_element_get_static_pad(ctx->audio_queue, "sink");
            if (q_aud_pad) {
                gst_pad_unlink(ctx->tee_audio_pad, q_aud_pad);
                gst_object_unref(q_aud_pad);
            }
        }
        // CORREÇÃO DO ERRO C2660: Passamos (Elemento, Pad)
        gst_element_release_request_pad(tee_audio, ctx->tee_audio_pad);
    }

    // Para elementos
    if (ctx->webrtcbin) gst_element_set_state(ctx->webrtcbin, GST_STATE_NULL);
    if (ctx->rtp_queue) gst_element_set_state(ctx->rtp_queue, GST_STATE_NULL);
    if (ctx->audio_queue) gst_element_set_state(ctx->audio_queue, GST_STATE_NULL);

    if (pipeline) {
        if (ctx->webrtcbin) gst_bin_remove(GST_BIN(pipeline), ctx->webrtcbin);
        if (ctx->rtp_queue) gst_bin_remove(GST_BIN(pipeline), ctx->rtp_queue);
        if (ctx->audio_queue) gst_bin_remove(GST_BIN(pipeline), ctx->audio_queue);
    }

    // Limpa os pads
    if (ctx->tee_pad) gst_object_unref(ctx->tee_pad);
    if (ctx->tee_audio_pad) gst_object_unref(ctx->tee_audio_pad);

    delete ctx;
    qDebug() << "✅ Cliente removido e recursos limpos.";
}

void ScreenStreamer::forceKeyframe() {
    if (encoder_element) {
        GstEvent* event = gst_video_event_new_downstream_force_key_unit(GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, TRUE, 0);
        gst_element_send_event(encoder_element, event);
    }
    else {
        // Fallback: tenta encontrar o encoder pelo tipo
        GstElement* enc = gst_bin_get_by_name(GST_BIN(pipeline), "vp8enc0"); // Nome padrão
        if (!enc) {
            // Tenta encontrar qualquer elemento vp8enc no pipeline
            GstIterator* it = gst_bin_iterate_elements(GST_BIN(pipeline));
            GValue value = G_VALUE_INIT;
            while (gst_iterator_next(it, &value) == GST_ITERATOR_OK) {
                GstElement* element = GST_ELEMENT(g_value_get_object(&value));
                if (GST_OBJECT_NAME(element) && g_strrstr(GST_OBJECT_NAME(element), "vp8enc")) {
                    enc = element;
                    gst_object_ref(enc);
                    break;
                }
                g_value_unset(&value);
            }
            gst_iterator_free(it);
        }

        if (enc) {
            GstEvent* event = gst_video_event_new_downstream_force_key_unit(GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, GST_CLOCK_TIME_NONE, TRUE, 0);
            gst_element_send_event(enc, event);
            gst_object_unref(enc);
        }
    }
}

void ScreenStreamer::handleSignalingMessage(int playerIndex, const QJsonObject& json)
{
    QMutexLocker locker(&m_clientsMutex);
    if (!m_clients.contains(playerIndex)) {
        qDebug() << "⚠️ Cliente" << playerIndex << "não encontrado para processar mensagem de sinalização";
        return;
    }

    ClientStreamContext* ctx = m_clients[playerIndex];
    QString type = json["type"].toString();
    qDebug() << "📨 Processando sinalização para Player" << playerIndex << ":" << type;

    if (type == "webrtc_answer") {
        QString sdp = json["sdp"].toString();
        qDebug() << "📝 Recebida resposta SDP do Player" << playerIndex << "Tamanho:" << sdp.length();

        GstSDPMessage* sdpMsg = nullptr;
        if (gst_sdp_message_new(&sdpMsg) != GST_SDP_OK) {
            qCritical() << "❌ Falha ao criar mensagem SDP";
            return;
        }

        if (gst_sdp_message_parse_buffer((guint8*)sdp.toUtf8().constData(), sdp.toUtf8().size(), sdpMsg) != GST_SDP_OK) {
            qCritical() << "❌ Falha ao parsear SDP da resposta";
            gst_sdp_message_free(sdpMsg);
            return;
        }

        GstWebRTCSessionDescription* answer = gst_webrtc_session_description_new(GST_WEBRTC_SDP_TYPE_ANSWER, sdpMsg);
        g_signal_emit_by_name(ctx->webrtcbin, "set-remote-description", answer, nullptr);
        gst_webrtc_session_description_free(answer);
        qDebug() << "✅ Resposta SDP aplicada para Player" << playerIndex;
    }
    else if (type == "webrtc_candidate") {
        QString candidate = json["candidate"].toString();
        int mlineIndex = json["sdpMLineIndex"].toInt();
        qDebug() << "🧊 Adicionando candidato ICE remoto para Player" << playerIndex << "MLine:" << mlineIndex;
        g_signal_emit_by_name(ctx->webrtcbin, "add-ice-candidate", mlineIndex, candidate.toUtf8().constData());
    }
    else {
        qDebug() << "⚠️ Tipo de mensagem de sinalização desconhecido para Player" << playerIndex << ":" << type;
    }
}

void ScreenStreamer::onNegotiationNeeded(GstElement* webrtc, gpointer user_data)
{
    // Pode deixar vazio ou logar, pois já chamamos create-offer manualmente no addClient
    // Se deixarmos aqui, pode gerar ofertas duplicadas.
    qDebug() << "📡 Negociação solicitada pelo GStreamer (ignorando para evitar loop)";
}

void ScreenStreamer::onOfferCreated(GstPromise* promise, gpointer user_data)
{
    CallbackData* cbData = static_cast<CallbackData*>(user_data);
    ScreenStreamer* self = cbData->self;
    int playerId = cbData->playerId;

    qDebug() << "🎯 onOfferCreated chamado para player" << playerId;

    const GstStructure* reply = gst_promise_get_reply(promise);
    if (!reply) {
        qCritical() << "❌ PROMISE SEM RESPOSTA para player" << playerId;
        gst_promise_unref(promise);
        return;
    }

    GstWebRTCSessionDescription* offer = nullptr;
    if (!gst_structure_get(reply, "offer", GST_TYPE_WEBRTC_SESSION_DESCRIPTION, &offer, nullptr)) {
        qCritical() << "❌ FALHA ao obter oferta do promise para player" << playerId;
        gst_promise_unref(promise);
        return;
    }

    gst_promise_unref(promise);

    self->m_clientsMutex.lock();
    if (self->m_clients.contains(playerId)) {
        GstElement* webrtc = self->m_clients[playerId]->webrtcbin;

        qDebug() << "📝 Configurando oferta local...";
        g_signal_emit_by_name(webrtc, "set-local-description", offer, nullptr);

        // Converte SDP para string
        gchar* sdp_string = gst_sdp_message_as_text(offer->sdp);
        QString sdp_qstr = QString::fromUtf8(sdp_string);
        qDebug() << "📨 Enviando oferta SDP para player" << playerId << "Tamanho:" << sdp_qstr.length();

        // Log das primeiras linhas do SDP para debug
        QStringList sdpLines = sdp_qstr.split('\n');
        for (int i = 0; i < qMin(5, sdpLines.size()); i++) {
            qDebug() << "   SDP Line" << i << ":" << sdpLines[i];
        }

        QJsonObject json;
        json["type"] = "webrtc_offer";
        json["sdp"] = sdp_qstr;

        emit self->sendSignalingMessage(playerId, json);
        g_free(sdp_string);

        qDebug() << "✅ Oferta enviada para player" << playerId;
    }
    else {
        qWarning() << "⚠️ Cliente" << playerId << "não encontrado ao enviar oferta";
    }
    self->m_clientsMutex.unlock();

    gst_webrtc_session_description_free(offer);
}

void ScreenStreamer::onIceCandidate(GstElement* webrtc, guint mline_index, gchar* candidate, gpointer user_data)
{
    Q_UNUSED(webrtc);
    CallbackData* cbData = static_cast<CallbackData*>(user_data);

    qDebug() << "🧊 Gerando ICE candidate para player" << cbData->playerId << "MLine:" << mline_index;

    QJsonObject json;
    json["type"] = "webrtc_candidate";
    json["candidate"] = QString::fromUtf8(candidate);
    json["sdpMLineIndex"] = (int)mline_index;
    json["sdpMid"] = "video0";

    emit cbData->self->sendSignalingMessage(cbData->playerId, json);
}

GstBusSyncReply ScreenStreamer::onBusMessage(GstBus* bus, GstMessage* msg, gpointer user_data)
{
    Q_UNUSED(bus);
    ScreenStreamer* self = static_cast<ScreenStreamer*>(user_data);

    switch (GST_MESSAGE_TYPE(msg)) {
    case GST_MESSAGE_ERROR: {
        GError* error;
        gchar* debug;
        gst_message_parse_error(msg, &error, &debug);
        qCritical() << "❌ ERRO no Pipeline:" << error->message;
        if (debug) {
            qCritical() << "Debug info:" << debug;
            g_free(debug);
        }
        g_error_free(error);

        QMetaObject::invokeMethod(self, [self]() {
            if (self->m_isStreamingEnabled) { // <--- CHECAGEM IMPORTANTE
                qDebug() << "🔄 Reiniciando pipeline (Erro detectado)...";
                self->stopMasterPipeline();
                QTimer::singleShot(1000, self, &ScreenStreamer::startMasterPipeline);
            }
            });
        break;
    }
    case GST_MESSAGE_WARNING: {
        GError* warning;
        gchar* debug;
        gst_message_parse_warning(msg, &warning, &debug);
        qWarning() << "⚠️ WARNING no Pipeline:" << warning->message;
        if (debug) {
            qWarning() << "Debug info:" << debug;
            g_free(debug);
        }
        g_error_free(warning);
        break;
    }
    case GST_MESSAGE_EOS:
        qDebug() << "🔚 Fim do stream (EOS) recebido";
        break;
    case GST_MESSAGE_STATE_CHANGED: {
        if (GST_MESSAGE_SRC(msg) == GST_OBJECT(self->pipeline)) {
            GstState old_state, new_state, pending_state;
            gst_message_parse_state_changed(msg, &old_state, &new_state, &pending_state);
            qDebug() << "🔄 Mudança de estado do Pipeline:" << old_state << "->" << new_state << "(pending:" << pending_state << ")";
        }
        break;
    }
    default:
        break;
    }

    return GST_BUS_PASS;
}