#include "CommunicationHub.h"

#include "communication/routing/MessageRouter.h"
#include "socket/SocketClient.h"

#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QUuid>

#include <chrono>

namespace {

QString normalizeMessageType(const QString& value)
{
    return value.trimmed().toLower().replace(QLatin1Char('-'), QLatin1Char('_'));
}

bool isGlobalTarget(const QString& module)
{
    return module.trimmed().compare(QStringLiteral("global"), Qt::CaseInsensitive) == 0;
}

QVariantMap objectPayload(const QVariantMap& envelope)
{
    QVariantMap value = envelope.value(QStringLiteral("value")).toMap();
    if (!value.isEmpty()) {
        return value;
    }

    value = envelope.value(QStringLiteral("payload")).toMap();
    if (!value.isEmpty()) {
        return value;
    }

    return envelope;
}

} // namespace

CommunicationHub::CommunicationHub(QObject* parent)
    : QObject(parent)
    , m_socketClient(std::make_unique<redis_dc::SocketClient>())
    , m_messageRouter(new MessageRouter(this))
    , m_clientInstanceId(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    qRegisterMetaType<StateSample>("StateSample");
    configureSocketCallbacks();
}

CommunicationHub::~CommunicationHub()
{
    stop();
}

void CommunicationHub::initialize()
{
    if (m_initialized) {
        return;
    }

    wireRouter();
    m_initialized = true;
    refreshHealthSnapshot();
}

void CommunicationHub::setServerEndpoint(const QString& host, quint16 port)
{
    const QString normalizedHost = host.trimmed();
    if (!normalizedHost.isEmpty()) {
        m_host = normalizedHost;
    }
    if (port > 0) {
        m_port = port;
    }
}

void CommunicationHub::setOutboundChannels(const QString& controlPublishChannel,
                                           const QString& ackChannel)
{
    if (!controlPublishChannel.trimmed().isEmpty()) {
        m_controlPublishChannel = controlPublishChannel.trimmed();
    }
    if (!ackChannel.trimmed().isEmpty()) {
        m_ackChannel = ackChannel.trimmed();
    }
}

void CommunicationHub::addRoutingChannel(const QString& channel)
{
    // Socket mode receives all messages from one connection, so channel
    // registration is intentionally kept as a compatibility no-op.
    Q_UNUSED(channel);
}

void CommunicationHub::sendActionRequest(const UiAction& action, bool loopbackToLocal)
{
    QVariantMap payload;
    payload.insert(QStringLiteral("category"), QStringLiteral("ActionRequest"));
    payload.insert(QStringLiteral("msgId"), action.actionId);
    payload.insert(QStringLiteral("actionId"), action.actionId);
    payload.insert(QStringLiteral("actionType"), UiAction::toString(action.actionType));
    payload.insert(QStringLiteral("module"), action.module);
    payload.insert(QStringLiteral("timestampMs"), action.timestampMs);
    payload.insert(QStringLiteral("payload"), action.payload);
    payload.insert(QStringLiteral("origin"), QStringLiteral("client"));
    payload.insert(QStringLiteral("senderId"), m_clientInstanceId);

    sendEnvelope(action.module, QStringLiteral("action_request"), payload);

    if (loopbackToLocal) {
        emit controlMessageReceived(action.module, payload);
    }
}

void CommunicationHub::sendResyncRequest(const QString& reason, bool loopbackToLocal)
{
    QVariantMap payload;
    payload.insert(QStringLiteral("category"), QStringLiteral("ResyncRequest"));
    payload.insert(QStringLiteral("msgId"), QUuid::createUuid().toString(QUuid::WithoutBraces));
    payload.insert(QStringLiteral("reason"), reason);
    payload.insert(QStringLiteral("timestampMs"), QDateTime::currentMSecsSinceEpoch());
    payload.insert(QStringLiteral("origin"), QStringLiteral("client"));
    payload.insert(QStringLiteral("senderId"), m_clientInstanceId);

    sendEnvelope(QStringLiteral("global"), QStringLiteral("resync_request"), payload);

    if (loopbackToLocal) {
        emit serverCommandReceived(QStringLiteral("resync_request"), payload);
    }
}

void CommunicationHub::sendTargetedMessage(const QString& targetName,
                                           const QString& messageType,
                                           const QVariantMap& payload)
{
    if (targetName.trimmed().isEmpty() || messageType.trimmed().isEmpty()) {
        return;
    }

    sendEnvelope(targetName.trimmed(), messageType.trimmed(), payload);
}

void CommunicationHub::start()
{
    if (m_started) {
        return;
    }

    m_started = true;
    if (!m_initialized) {
        initialize();
    }

    redis_dc::SocketClient::ReconnectOptions reconnectOptions;
    reconnectOptions.enabled = true;
    reconnectOptions.delay = std::chrono::milliseconds(1000);
    reconnectOptions.maxAttempts = 0;
    m_socketClient->setReconnectOptions(reconnectOptions);

    qInfo().noquote()
        << QStringLiteral("[Socket] connecting to %1:%2 ...").arg(m_host).arg(m_port);
    if (!m_socketClient->connect(m_host.toStdString(), m_port)) {
        onSocketDisconnected();
        emitIssue(
            QStringLiteral("SocketClient"),
            QStringLiteral("warning"),
            QStringLiteral("SOCKET_CONNECT_FAILED"),
            QStringLiteral("Socket is not connected after the connect attempt"),
            {{QStringLiteral("host"), m_host}, {QStringLiteral("port"), m_port}});
    }
}

void CommunicationHub::stop()
{
    m_started = false;
    if (m_socketClient) {
        m_socketClient->disconnect();
    }
}

QString CommunicationHub::getConnectionStateName() const
{
    return m_connectionState;
}

QVariantMap CommunicationHub::getHealthSnapshot() const
{
    return m_healthSnapshot;
}

void CommunicationHub::wireRouter()
{
    connect(m_messageRouter, &MessageRouter::actionRequestRouted,
            this, [this](const QString& module, const QVariantMap& payload) {
                if (payload.value(QStringLiteral("senderId")).toString() == m_clientInstanceId) {
                    return;
                }
                m_lastControlMessageMs = QDateTime::currentMSecsSinceEpoch();
                ++m_receivedControlCount;
                emit controlMessageReceived(module, payload);
                refreshHealthSnapshot();
            });
    connect(m_messageRouter, &MessageRouter::serverCommandRouted,
            this, [this](const QString& commandType, const QVariantMap& payload) {
                if (payload.value(QStringLiteral("senderId")).toString() == m_clientInstanceId) {
                    return;
                }
                m_lastControlMessageMs = QDateTime::currentMSecsSinceEpoch();
                ++m_receivedControlCount;
                emit serverCommandReceived(commandType, payload);
                refreshHealthSnapshot();
            });
    connect(m_messageRouter, &MessageRouter::heartbeatReceived,
            this, [this](const QVariantMap&) {
                m_lastHeartbeatMs = QDateTime::currentMSecsSinceEpoch();
                emit heartbeatReceived();
                refreshHealthSnapshot();
            });
    connect(m_messageRouter, &MessageRouter::resyncRequestReceived,
            this, [this](const QVariantMap& payload) {
                m_lastControlMessageMs = QDateTime::currentMSecsSinceEpoch();
                ++m_receivedControlCount;
                emit serverCommandReceived(QStringLiteral("resync_request"), payload);
                refreshHealthSnapshot();
            });
    connect(m_messageRouter, &MessageRouter::resyncResponseReceived,
            this, [this](const QVariantMap& payload) {
                m_lastControlMessageMs = QDateTime::currentMSecsSinceEpoch();
                ++m_receivedControlCount;
                emit serverCommandReceived(QStringLiteral("resync_response"), payload);
                refreshHealthSnapshot();
            });
    connect(m_messageRouter, &MessageRouter::routingError,
            this, [this](const QString& errorMessage) {
                ++m_routingErrorCount;
                emitIssue(QStringLiteral("MessageRouter"),
                          QStringLiteral("warning"),
                          QStringLiteral("COMM_ROUTING_ERROR"),
                          errorMessage);
            });
}

void CommunicationHub::configureSocketCallbacks()
{
    m_socketClient->setConnectedCallback([this]() {
        QMetaObject::invokeMethod(this, [this]() { onSocketConnected(); }, Qt::QueuedConnection);
    });
    m_socketClient->setDisconnectCallback([this]() {
        QMetaObject::invokeMethod(this, [this]() { onSocketDisconnected(); }, Qt::QueuedConnection);
    });
    m_socketClient->setErrorCallback([this](const std::string& message) {
        const QString errorMessage = QString::fromStdString(message);
        QMetaObject::invokeMethod(this, [this, errorMessage]() { onSocketError(errorMessage); },
                                  Qt::QueuedConnection);
    });
    m_socketClient->setRawMessageCallback([this](const std::string& message) {
        const QByteArray raw = QByteArray::fromStdString(message);
        QMetaObject::invokeMethod(this, [this, raw]() { onSocketRawMessage(raw); },
                                  Qt::QueuedConnection);
    });
}

void CommunicationHub::onSocketConnected()
{
    m_connectionState = QStringLiteral("Connected");
    emit connectionStateChanged(m_connectionState);
    refreshHealthSnapshot();
}

void CommunicationHub::onSocketDisconnected()
{
    m_connectionState = m_started ? QStringLiteral("Reconnecting") : QStringLiteral("Disconnected");
    emit connectionStateChanged(m_connectionState);
    refreshHealthSnapshot();
}

void CommunicationHub::onSocketError(const QString& errorMessage)
{
    ++m_transportErrorCount;
    emit communicationError(QStringLiteral("SocketClient"), errorMessage);
    emitIssue(QStringLiteral("SocketClient"),
              QStringLiteral("warning"),
              QStringLiteral("SOCKET_TRANSPORT_ERROR"),
              errorMessage);
}

void CommunicationHub::onSocketRawMessage(const QByteArray& message)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(message, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        ++m_routingErrorCount;
        emitIssue(QStringLiteral("SocketClient"),
                  QStringLiteral("warning"),
                  QStringLiteral("SOCKET_INVALID_JSON"),
                  QStringLiteral("Invalid socket JSON message: %1").arg(parseError.errorString()));
        return;
    }

    const QVariantMap envelope = doc.object().toVariantMap();
    if (envelope.contains(QStringLiteral("category"))) {
        m_messageRouter->routeIncomingMessage(message);
        return;
    }

    routeEnvelopeMessage(envelope, message);
}

void CommunicationHub::routeEnvelopeMessage(const QVariantMap& envelope, const QByteArray& rawMessage)
{
    Q_UNUSED(rawMessage);

    const QString module = envelope.value(QStringLiteral("module")).toString();
    const QString type = normalizeMessageType(envelope.value(QStringLiteral("type")).toString());
    QVariantMap payload = objectPayload(envelope);

    if (type.isEmpty()) {
        ++m_routingErrorCount;
        emitIssue(QStringLiteral("SocketClient"),
                  QStringLiteral("warning"),
                  QStringLiteral("SOCKET_MESSAGE_TYPE_MISSING"),
                  QStringLiteral("Socket envelope is missing type"),
                  envelope);
        return;
    }

    if (!payload.contains(QStringLiteral("module")) && !module.isEmpty()) {
        payload.insert(QStringLiteral("module"), module);
    }

    if (type == QStringLiteral("heartbeat")) {
        m_lastHeartbeatMs = QDateTime::currentMSecsSinceEpoch();
        emit heartbeatReceived();
        refreshHealthSnapshot();
        return;
    }

    if (type == QStringLiteral("action") ||
        type == QStringLiteral("action_request") ||
        type == QStringLiteral("ui_action")) {
        m_lastControlMessageMs = QDateTime::currentMSecsSinceEpoch();
        ++m_receivedControlCount;
        emit controlMessageReceived(isGlobalTarget(module) ? QString() : module, payload);
        refreshHealthSnapshot();
        return;
    }

    if (type == QStringLiteral("command") ||
        type == QStringLiteral("server_command")) {
        m_lastControlMessageMs = QDateTime::currentMSecsSinceEpoch();
        ++m_receivedControlCount;
        QString commandType = payload.value(QStringLiteral("commandType")).toString();
        if (commandType.isEmpty()) {
            commandType = payload.value(QStringLiteral("command")).toString();
        }
        emit serverCommandReceived(commandType, payload);
        refreshHealthSnapshot();
        return;
    }

    if (type == QStringLiteral("resync_request") ||
        type == QStringLiteral("resync_response")) {
        m_lastControlMessageMs = QDateTime::currentMSecsSinceEpoch();
        ++m_receivedControlCount;
        emit serverCommandReceived(type, payload);
        refreshHealthSnapshot();
        return;
    }

    const StateSample sample = StateSample::create(
        QStringLiteral("socket"),
        isGlobalTarget(module) ? QStringLiteral("global") : module,
        type,
        payload);
    m_lastStateSampleMs = sample.timestampMs;
    ++m_receivedSampleCount;
    emit stateSampleReceived(sample);
    refreshHealthSnapshot();
}

bool CommunicationHub::sendJson(const QVariantMap& payload)
{
    if (!m_socketClient || !m_socketClient->isConnected()) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromVariant(payload);
    const bool sent = m_socketClient->send(QString::fromUtf8(doc.toJson(QJsonDocument::Compact)).toStdString());
    if (sent) {
        ++m_sentMessageCount;
        refreshHealthSnapshot();
    }
    return sent;
}

bool CommunicationHub::sendEnvelope(const QString& module, const QString& type, const QVariantMap& value)
{
    QVariantMap envelope;
    envelope.insert(QStringLiteral("module"), module);
    envelope.insert(QStringLiteral("type"), type);
    envelope.insert(QStringLiteral("value"), value);
    return sendJson(envelope);
}

void CommunicationHub::emitIssue(const QString& source, const QString& severity,
                                 const QString& errorCode, const QString& errorMessage,
                                 const QVariantMap& context)
{
    emit communicationIssue(source, severity, errorCode, errorMessage, context);
    refreshHealthSnapshot();
}

void CommunicationHub::refreshHealthSnapshot()
{
    QVariantMap snapshot;
    snapshot.insert(QStringLiteral("healthState"),
                    m_connectionState == QStringLiteral("Connected")
                        ? ((m_routingErrorCount > 0 || m_transportErrorCount > 0)
                               ? QStringLiteral("degraded")
                               : QStringLiteral("healthy"))
                        : QStringLiteral("offline"));
    snapshot.insert(QStringLiteral("connectionState"), m_connectionState);
    snapshot.insert(QStringLiteral("host"), m_host);
    snapshot.insert(QStringLiteral("port"), m_port);
    snapshot.insert(QStringLiteral("lastHeartbeatMs"), m_lastHeartbeatMs);
    snapshot.insert(QStringLiteral("lastControlMessageMs"), m_lastControlMessageMs);
    snapshot.insert(QStringLiteral("lastStateSampleMs"), m_lastStateSampleMs);
    snapshot.insert(QStringLiteral("routingErrors"), m_routingErrorCount);
    snapshot.insert(QStringLiteral("transportErrors"), m_transportErrorCount);
    snapshot.insert(QStringLiteral("receivedControlCount"), m_receivedControlCount);
    snapshot.insert(QStringLiteral("receivedSampleCount"), m_receivedSampleCount);
    snapshot.insert(QStringLiteral("sentMessageCount"), m_sentMessageCount);

    if (snapshot != m_healthSnapshot) {
        m_healthSnapshot = snapshot;
        emit healthSnapshotChanged(m_healthSnapshot);
    } else {
        m_healthSnapshot = snapshot;
    }
}
