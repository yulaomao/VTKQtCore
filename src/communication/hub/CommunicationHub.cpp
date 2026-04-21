#include "CommunicationHub.h"

#include "communication/datasource/GlobalPollingPlan.h"
#include "communication/datasource/PollingSource.h"
#include "communication/datasource/SubscriptionSource.h"
#include "communication/redis/RedisGateway.h"
#include "communication/redis/RedisPollingWorker.h"
#include "communication/routing/MessageRouter.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QThread>
#include <QTimer>
#include <QUuid>

namespace {

QString connectionStateName(RedisGateway::ConnectionState state)
{
    switch (state) {
    case RedisGateway::Connected:
        return QStringLiteral("Connected");
    case RedisGateway::Reconnecting:
        return QStringLiteral("Reconnecting");
    case RedisGateway::Disconnected:
    default:
        return QStringLiteral("Disconnected");
    }
}

} // namespace

CommunicationHub::CommunicationHub(RedisGateway* gateway, QObject* parent)
    : QObject(parent)
    , m_redisGateway(gateway)
    , m_messageRouter(new MessageRouter(this))
    , m_pollingSource(new PollingSource(QStringLiteral("polling_source")))
    , m_pollingThread(new QThread(this))
    , m_outboundRetryTimer(new QTimer(this))
    , m_lastConnectionState(gateway ? gateway->getConnectionState() : RedisGateway::Disconnected)
    , m_clientInstanceId(QUuid::createUuid().toString(QUuid::WithoutBraces))
{
    qRegisterMetaType<StateSample>("StateSample");
    qRegisterMetaType<GlobalPollingPlan>("GlobalPollingPlan");

    m_pollingThread->setObjectName(QStringLiteral("CommunicationHubPollingThread"));
    m_pollingSource->moveToThread(m_pollingThread);
    connect(m_pollingThread, &QThread::finished,
            m_pollingSource, &QObject::deleteLater);

    // Create a persistent polling connection that lives on the polling thread.
    // This replaces the old asyncReadKey approach (which spawned a new thread
    // + TCP connection for every single GET call).
    m_pollingWorker = new RedisPollingWorker(QString(), 0);
    m_pollingWorker->moveToThread(m_pollingThread);
    connect(m_pollingThread, &QThread::finished,
            m_pollingWorker, &QObject::deleteLater);

    m_pollingThread->start();

    m_outboundRetryTimer->setInterval(m_outboundRetryTickMs);
    connect(m_outboundRetryTimer, &QTimer::timeout,
            this, &CommunicationHub::onOutboundRetryTimeout);
    m_outboundRetryTimer->start();
}

CommunicationHub::~CommunicationHub()
{
    stop();

    if (m_outboundRetryTimer) {
        m_outboundRetryTimer->stop();
    }

    if (m_pollingThread && m_pollingThread->isRunning()) {
        stopPollingTransport(true);
        m_pollingThread->quit();
        m_pollingThread->wait();
    }
}

void CommunicationHub::initialize()
{
    addRoutingChannel(m_ackChannel);

    if (m_redisGateway) {
        connect(m_redisGateway, &RedisGateway::messageReceived,
                this, &CommunicationHub::onGatewayMessageReceived);
    }

    connect(m_messageRouter, &MessageRouter::ackReceived,
            this, &CommunicationHub::onAckReceived);
    connect(m_messageRouter, &MessageRouter::actionRequestRouted,
            this, [this](const QString& module, const QVariantMap& payload) {
                if (isSelfOriginated(payload)) {
                    return;
                }
                m_lastControlMessageMs = QDateTime::currentMSecsSinceEpoch();
                ++m_receivedControlCount;
                if (shouldAck(QStringLiteral("ActionRequest"), payload)) {
                    publishAck(QStringLiteral("ActionRequest"), payload);
                }
                emit controlMessageReceived(module, payload);
                refreshHealthSnapshot();
            });
    connect(m_messageRouter, &MessageRouter::serverCommandRouted,
            this, [this](const QString& commandType, const QVariantMap& payload) {
                if (isSelfOriginated(payload)) {
                    return;
                }
                m_lastControlMessageMs = QDateTime::currentMSecsSinceEpoch();
                ++m_receivedControlCount;
                if (shouldAck(QStringLiteral("ServerCommand"), payload)) {
                    publishAck(QStringLiteral("ServerCommand"), payload);
                }
                emit serverCommandReceived(commandType, payload);
                refreshHealthSnapshot();
            });
    connect(m_messageRouter, &MessageRouter::resyncRequestReceived,
            this, [this](const QVariantMap& payload) {
                if (isSelfOriginated(payload)) {
                    return;
                }
                m_lastControlMessageMs = QDateTime::currentMSecsSinceEpoch();
                ++m_receivedControlCount;
                if (shouldAck(QStringLiteral("ResyncRequest"), payload)) {
                    publishAck(QStringLiteral("ResyncRequest"), payload);
                }
                emit serverCommandReceived(QStringLiteral("resync_request"), payload);
                refreshHealthSnapshot();
            });
    connect(m_messageRouter, &MessageRouter::resyncResponseReceived,
            this, [this](const QVariantMap& payload) {
                if (isSelfOriginated(payload)) {
                    return;
                }
                m_lastControlMessageMs = QDateTime::currentMSecsSinceEpoch();
                ++m_receivedControlCount;
                if (shouldAck(QStringLiteral("ResyncResponse"), payload)) {
                    publishAck(QStringLiteral("ResyncResponse"), payload);
                }
                emit serverCommandReceived(QStringLiteral("resync_response"), payload);
                refreshHealthSnapshot();
            });
    connect(m_messageRouter, &MessageRouter::heartbeatReceived,
            this, [this](const QVariantMap& payload) {
                m_lastHeartbeatMs = QDateTime::currentMSecsSinceEpoch();
                if (shouldAck(QStringLiteral("Heartbeat"), payload)) {
                    publishAck(QStringLiteral("Heartbeat"), payload);
                }
                emit heartbeatReceived();
                refreshHealthSnapshot();
            });
    connect(m_messageRouter, &MessageRouter::routingError,
            this, [this](const QString& errorMessage) {
                ++m_routingErrorCount;
                emitIssue(
                    QStringLiteral("MessageRouter"),
                    QStringLiteral("warning"),
                    QStringLiteral("COMM_ROUTING_ERROR"),
                    errorMessage);
            });

    connect(m_pollingSource, &PollingSource::sampleReady,
            this, [this](const StateSample& sample) {
                m_lastStateSampleMs = sample.timestampMs;
                ++m_receivedSampleCount;
                emit stateSampleReceived(sample);
                refreshHealthSnapshot();
            });
    connect(m_pollingSource, &PollingSource::sourceError,
            this, [this](const QString& sourceId, const QString& errorMessage) {
                ++m_datasourceErrorCount;
                emitIssue(
                    sourceId,
                    QStringLiteral("warning"),
                    QStringLiteral("DATASOURCE_POLLING_ERROR"),
                    errorMessage,
                    {{QStringLiteral("layer"), QStringLiteral("polling")}});
            });

    if (m_redisGateway) {
        // Route poll requests directly to the persistent polling worker that
        // lives on the polling thread.  Both objects share the same thread, so
        // this is a Qt::DirectConnection and no extra thread is ever spawned.
        connect(m_pollingSource, &PollingSource::batchPollRequested,
                m_pollingWorker, &RedisPollingWorker::readKeys);
        connect(m_pollingWorker, &RedisPollingWorker::keyValuesReceived,
                m_pollingSource, &PollingSource::onBatchPollResult);
    }

    if (m_redisGateway) {
        connect(m_redisGateway, &RedisGateway::errorOccurred,
                this, [this](const QString& errorMessage) {
                    emitIssue(
                        QStringLiteral("RedisGateway"),
                        QStringLiteral("error"),
                        QStringLiteral("REDIS_TRANSPORT_ERROR"),
                        errorMessage);
                });
        connect(m_redisGateway, &RedisGateway::connectionStateChanged,
                this, &CommunicationHub::onGatewayConnectionStateChanged);
    }

    refreshHealthSnapshot();
}

void CommunicationHub::addRoutingChannel(const QString& channel)
{
    if (channel.isEmpty() || m_routingChannels.contains(channel)) {
        return;
    }

    m_routingChannels.insert(channel);
    if (m_started && m_redisGateway && m_redisGateway->getConnectionState() == RedisGateway::Connected) {
        m_redisGateway->subscribe(channel);
    }
    refreshHealthSnapshot();
}

void CommunicationHub::addSubscriptionSource(SubscriptionSource* source)
{
    if (!source || hasSubscriptionSource(source->getSourceId())) {
        return;
    }

    source->setParent(this);
    m_subscriptionSources.append(source);
    connectSource(source);

    if (m_started && m_redisGateway && m_redisGateway->getConnectionState() == RedisGateway::Connected) {
        if (!source->isRunning()) {
            source->start();
        }
        m_redisGateway->subscribe(source->getChannel());
    }
}

void CommunicationHub::setGlobalPollingPlan(const GlobalPollingPlan& plan)
{
    QMetaObject::invokeMethod(
        m_pollingSource,
        "configurePlan",
        Qt::BlockingQueuedConnection,
        Q_ARG(GlobalPollingPlan, plan));

    // Forward the DB number so the worker issues SELECT before the first MGET.
    QMetaObject::invokeMethod(m_pollingWorker, "setDb",
                              Qt::QueuedConnection,
                              Q_ARG(int, plan.getDb()));

    m_hasActivePollingPlan = plan.isActive() && !plan.getRedisKeys().isEmpty();
    m_globalPollingKeyCount = plan.getRedisKeys().size();

    if (!m_hasActivePollingPlan) {
        if (m_pollingTransportRunning) {
            stopPollingTransport();
        }
    } else if (m_started && m_redisGateway &&
               m_redisGateway->getConnectionState() == RedisGateway::Connected &&
               !m_pollingTransportRunning) {
        startPollingTransport();
    }

    refreshHealthSnapshot();
}

void CommunicationHub::setOutboundChannels(const QString& controlPublishChannel,
                                           const QString& ackChannel)
{
    if (!controlPublishChannel.isEmpty()) {
        m_controlPublishChannel = controlPublishChannel;
    }
    if (!ackChannel.isEmpty()) {
        m_ackChannel = ackChannel;
        addRoutingChannel(m_ackChannel);
    }
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
    payload.insert(QStringLiteral("requireAck"), true);
    payload.insert(QStringLiteral("origin"), QStringLiteral("client"));
    payload.insert(QStringLiteral("senderId"), m_clientInstanceId);
    payload.insert(QStringLiteral("seq"), m_nextOutboundSeq++);

    publishJson(m_controlPublishChannel, payload);

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
    payload.insert(QStringLiteral("requireAck"), true);
    payload.insert(QStringLiteral("origin"), QStringLiteral("client"));
    payload.insert(QStringLiteral("senderId"), m_clientInstanceId);
    payload.insert(QStringLiteral("seq"), m_nextOutboundSeq++);

    publishJson(m_controlPublishChannel, payload);
    ++m_resyncRequestCount;
    refreshHealthSnapshot();

    if (loopbackToLocal) {
        emit serverCommandReceived(QStringLiteral("resync_request"), payload);
    }
}

void CommunicationHub::start()
{
    m_started = true;
    if (m_redisGateway && m_redisGateway->getConnectionState() == RedisGateway::Connected) {
        activateTransport();
    }
}

void CommunicationHub::stop()
{
    deactivateTransport();
    m_started = false;
}

QString CommunicationHub::getConnectionStateName() const
{
    if (!m_redisGateway) {
        return QStringLiteral("Disconnected");
    }

    return connectionStateName(m_redisGateway->getConnectionState());
}

QVariantMap CommunicationHub::getHealthSnapshot() const
{
    return m_healthSnapshot;
}

void CommunicationHub::onAckReceived(const QVariantMap& payload)
{
    if (isSelfOriginated(payload)) {
        return;
    }

    const QString msgId = payload.value(QStringLiteral("msgId")).toString();
    if (msgId.isEmpty()) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_inflightReliableMessages.contains(msgId)) {
        m_inflightReliableMessages.remove(msgId);
        ++m_ackConfirmedCount;
    }

    m_confirmedOutboundWindow.insert(msgId, now);
    cleanupIdempotencyWindow(now);
    refreshHealthSnapshot();
}

void CommunicationHub::onGatewayMessageReceived(const QString& channel, const QByteArray& message)
{
    if (m_routingChannels.contains(channel)) {
        m_messageRouter->routeIncomingMessage(message);
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(message, &parseError);
    QVariantMap data;
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        data = doc.object().toVariantMap();
    }

    for (SubscriptionSource* source : m_subscriptionSources) {
        source->onMessageReceived(channel, data);
    }
}

void CommunicationHub::connectSource(SubscriptionSource* source)
{
    connect(source, &SubscriptionSource::sampleReady,
            this, [this](const StateSample& sample) {
                m_lastStateSampleMs = sample.timestampMs;
                ++m_receivedSampleCount;
                emit stateSampleReceived(sample);
                refreshHealthSnapshot();
            });
    connect(source, &SubscriptionSource::sourceError,
            this, [this](const QString& sourceId, const QString& errorMessage) {
                ++m_datasourceErrorCount;
                emitIssue(
                    sourceId,
                    QStringLiteral("warning"),
                    QStringLiteral("DATASOURCE_SUBSCRIPTION_ERROR"),
                    errorMessage,
                    {{QStringLiteral("layer"), QStringLiteral("subscription")}});
            });
}

void CommunicationHub::onGatewayConnectionStateChanged(RedisGateway::ConnectionState state)
{
    const bool recovered = m_started && state == RedisGateway::Connected &&
        (m_lastConnectionState == RedisGateway::Reconnecting ||
         m_lastConnectionState == RedisGateway::Disconnected);

    if (m_started) {
        if (state == RedisGateway::Connected) {
            activateTransport();
        } else {
            deactivateTransport();
        }
    }

    emit connectionStateChanged(connectionStateName(state));

    if (state == RedisGateway::Connected) {
        resendInflightMessages();
        flushOutboundQueue();
    }

    if (recovered) {
        ++m_reconnectCount;
        sendResyncRequest(QStringLiteral("communication_recovered"), true);
    }

    m_lastConnectionState = state;
    refreshHealthSnapshot();
}

void CommunicationHub::onOutboundRetryTimeout()
{
    if (!m_started || !m_redisGateway || m_redisGateway->getConnectionState() != RedisGateway::Connected) {
        return;
    }

    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    cleanupIdempotencyWindow(now);

    const QStringList inflightIds = m_inflightReliableMessages.keys();
    for (const QString& msgId : inflightIds) {
        OutboundControlMessage message = m_inflightReliableMessages.value(msgId);
        if (message.lastSentMs <= 0 || now - message.lastSentMs < m_outboundAckTimeoutMs) {
            continue;
        }

        m_inflightReliableMessages.remove(msgId);
        if (message.retryCount >= m_outboundRetryLimit) {
            ++m_ackTimeoutCount;
            ++m_droppedReliableCount;
            emitIssue(
                QStringLiteral("CommunicationHub"),
                QStringLiteral("warning"),
                QStringLiteral("COMM_ACK_TIMEOUT"),
                QStringLiteral("Reliable outbound message '%1' timed out waiting for ack").arg(msgId),
                {{QStringLiteral("msgId"), msgId},
                 {QStringLiteral("retryCount"), message.retryCount}});
            continue;
        }

        ++message.retryCount;
        dispatchOutboundMessage(message);
    }

    refreshHealthSnapshot();
}

void CommunicationHub::activateTransport()
{
    if (!m_redisGateway || m_redisGateway->getConnectionState() != RedisGateway::Connected) {
        return;
    }

    // Keep the polling worker's connection parameters in sync with the gateway.
    QMetaObject::invokeMethod(m_pollingWorker, "setConnection",
                              Qt::QueuedConnection,
                              Q_ARG(QString, m_redisGateway->getHost()),
                              Q_ARG(int, m_redisGateway->getPort()));

    for (SubscriptionSource* source : m_subscriptionSources) {
        if (!source->isRunning()) {
            source->start();
        }
        m_redisGateway->subscribe(source->getChannel());
    }

    for (const QString& channel : m_routingChannels) {
        m_redisGateway->subscribe(channel);
    }

    startPollingTransport();
}

void CommunicationHub::deactivateTransport()
{
    stopPollingTransport();

    if (!m_redisGateway) {
        return;
    }

    for (SubscriptionSource* source : m_subscriptionSources) {
        if (source->isRunning()) {
            source->stop();
        }
        m_redisGateway->unsubscribe(source->getChannel());
    }

    for (const QString& channel : m_routingChannels) {
        m_redisGateway->unsubscribe(channel);
    }
}

void CommunicationHub::startPollingTransport(bool blocking)
{
    if (!m_pollingSource || !m_pollingThread || !m_pollingThread->isRunning() ||
        m_pollingTransportRunning || !m_hasActivePollingPlan) {
        return;
    }

    if (QThread::currentThread() == m_pollingThread) {
        m_pollingSource->start();
    } else {
        QMetaObject::invokeMethod(
            m_pollingSource,
            "start",
            blocking ? Qt::BlockingQueuedConnection : Qt::QueuedConnection);
    }
    m_pollingTransportRunning = true;
}

void CommunicationHub::stopPollingTransport(bool blocking)
{
    if (!m_pollingSource || !m_pollingTransportRunning) {
        return;
    }

    if (QThread::currentThread() == m_pollingThread) {
        m_pollingSource->stop();
    } else {
        QMetaObject::invokeMethod(
            m_pollingSource,
            "stop",
            blocking ? Qt::BlockingQueuedConnection : Qt::QueuedConnection);
    }
    m_pollingTransportRunning = false;
}

void CommunicationHub::publishAck(const QString& category, const QVariantMap& payload,
                                  const QString& status)
{
    const QString msgId = payload.value(QStringLiteral("msgId")).toString();
    if (msgId.isEmpty()) {
        return;
    }

    QVariantMap ackPayload;
    ackPayload.insert(QStringLiteral("category"), QStringLiteral("Ack"));
    ackPayload.insert(QStringLiteral("msgId"), msgId);
    ackPayload.insert(QStringLiteral("ackedCategory"), category);
    ackPayload.insert(QStringLiteral("status"), status);
    ackPayload.insert(QStringLiteral("timestampMs"), QDateTime::currentMSecsSinceEpoch());
    ackPayload.insert(QStringLiteral("module"), payload.value(QStringLiteral("module")).toString());
    ackPayload.insert(QStringLiteral("commandType"), payload.value(QStringLiteral("commandType")).toString());
    ackPayload.insert(QStringLiteral("origin"), QStringLiteral("client"));
    ackPayload.insert(QStringLiteral("senderId"), m_clientInstanceId);
    ackPayload.insert(QStringLiteral("seq"), m_nextOutboundSeq++);
    publishJson(m_ackChannel, ackPayload);
    ++m_ackCount;
    refreshHealthSnapshot();
}

bool CommunicationHub::publishJson(const QString& channel, const QVariantMap& payload)
{
    if (!m_redisGateway || channel.isEmpty()) {
        return false;
    }

    QVariantMap normalizedPayload = payload;
    if (!normalizedPayload.contains(QStringLiteral("senderId"))) {
        normalizedPayload.insert(QStringLiteral("senderId"), m_clientInstanceId);
    }
    if (!normalizedPayload.contains(QStringLiteral("origin"))) {
        normalizedPayload.insert(QStringLiteral("origin"), QStringLiteral("client"));
    }

    OutboundControlMessage message;
    message.channel = channel;
    message.payload = normalizedPayload;
    message.requireAck = requiresReliableAck(channel, normalizedPayload);
    message.msgId = normalizedPayload.value(QStringLiteral("msgId")).toString();
    message.firstQueuedMs = QDateTime::currentMSecsSinceEpoch();

    if (message.requireAck && message.msgId.isEmpty()) {
        message.msgId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        message.payload.insert(QStringLiteral("msgId"), message.msgId);
    }

    if (message.requireAck) {
        message.payload.insert(QStringLiteral("requireAck"), true);
        cleanupIdempotencyWindow(QDateTime::currentMSecsSinceEpoch());

        if (m_confirmedOutboundWindow.contains(message.msgId) ||
            m_inflightReliableMessages.contains(message.msgId)) {
            return true;
        }

        for (const OutboundControlMessage& queuedMessage : m_outboundQueue) {
            if (queuedMessage.msgId == message.msgId) {
                return true;
            }
        }
    }

    if (m_redisGateway->getConnectionState() != RedisGateway::Connected) {
        m_outboundQueue.append(message);
        refreshHealthSnapshot();
        return false;
    }

    dispatchOutboundMessage(message);
    return true;
}

void CommunicationHub::dispatchOutboundMessage(OutboundControlMessage message)
{
    if (!m_redisGateway || m_redisGateway->getConnectionState() != RedisGateway::Connected) {
        m_outboundQueue.append(message);
        return;
    }

    message.lastSentMs = QDateTime::currentMSecsSinceEpoch();
    const QJsonDocument doc = QJsonDocument::fromVariant(message.payload);
    m_redisGateway->publish(message.channel, doc.toJson(QJsonDocument::Compact));

    if (message.requireAck && !message.msgId.isEmpty()) {
        m_inflightReliableMessages.insert(message.msgId, message);
    }
}

void CommunicationHub::flushOutboundQueue()
{
    if (!m_redisGateway || m_redisGateway->getConnectionState() != RedisGateway::Connected) {
        return;
    }

    QList<OutboundControlMessage> retryQueue;
    while (!m_outboundQueue.isEmpty()) {
        OutboundControlMessage message = m_outboundQueue.takeFirst();
        if (message.requireAck && !message.msgId.isEmpty()) {
            if (m_confirmedOutboundWindow.contains(message.msgId) ||
                m_inflightReliableMessages.contains(message.msgId)) {
                continue;
            }
        }

        dispatchOutboundMessage(message);
        if (m_redisGateway->getConnectionState() != RedisGateway::Connected) {
            ++message.retryCount;
            if (message.retryCount < m_outboundRetryLimit) {
                retryQueue.append(message);
            } else {
                ++m_droppedReliableCount;
            }
        }
    }
    m_outboundQueue = retryQueue;
    refreshHealthSnapshot();
}

void CommunicationHub::resendInflightMessages()
{
    if (!m_redisGateway || m_redisGateway->getConnectionState() != RedisGateway::Connected) {
        return;
    }

    const QStringList inflightIds = m_inflightReliableMessages.keys();
    for (const QString& msgId : inflightIds) {
        OutboundControlMessage message = m_inflightReliableMessages.take(msgId);
        if (message.retryCount >= m_outboundRetryLimit) {
            ++m_droppedReliableCount;
            emitIssue(
                QStringLiteral("CommunicationHub"),
                QStringLiteral("warning"),
                QStringLiteral("COMM_ACK_RETRY_EXHAUSTED"),
                QStringLiteral("Dropped reliable outbound message '%1' after retry limit").arg(msgId),
                {{QStringLiteral("msgId"), msgId}});
            continue;
        }

        ++message.retryCount;
        dispatchOutboundMessage(message);
    }
}

bool CommunicationHub::shouldAck(const QString& category, const QVariantMap& payload) const
{
    if (payload.value(QStringLiteral("requireAck")).toBool()) {
        return true;
    }

    return category == QStringLiteral("ActionRequest") ||
           category == QStringLiteral("ServerCommand") ||
           category == QStringLiteral("ResyncRequest") ||
           category == QStringLiteral("ResyncResponse");
}

bool CommunicationHub::requiresReliableAck(const QString& channel, const QVariantMap& payload) const
{
    if (channel == m_ackChannel || payload.value(QStringLiteral("category")).toString() == QStringLiteral("Ack")) {
        return false;
    }

    if (payload.value(QStringLiteral("requireAck")).toBool()) {
        return true;
    }

    return channel == m_controlPublishChannel;
}

bool CommunicationHub::isSelfOriginated(const QVariantMap& payload) const
{
    return payload.value(QStringLiteral("senderId")).toString() == m_clientInstanceId;
}

void CommunicationHub::emitIssue(const QString& source, const QString& severity,
                                 const QString& errorCode, const QString& errorMessage,
                                 const QVariantMap& context)
{
    emit communicationIssue(source, severity, errorCode, errorMessage, context);
    refreshHealthSnapshot();
}

void CommunicationHub::cleanupIdempotencyWindow(qint64 nowMs)
{
    auto it = m_confirmedOutboundWindow.begin();
    while (it != m_confirmedOutboundWindow.end()) {
        if (nowMs - it.value() >= m_idempotencyWindowMs) {
            it = m_confirmedOutboundWindow.erase(it);
        } else {
            ++it;
        }
    }
}

void CommunicationHub::refreshHealthSnapshot()
{
    cleanupIdempotencyWindow(QDateTime::currentMSecsSinceEpoch());

    QVariantMap snapshot;
    snapshot.insert(QStringLiteral("healthState"),
                    m_lastConnectionState == RedisGateway::Disconnected
                        ? QStringLiteral("offline")
                        : ((m_routingErrorCount > 0 || m_datasourceErrorCount > 0 ||
                            m_ackTimeoutCount > 0)
                               ? QStringLiteral("degraded")
                               : QStringLiteral("healthy")));
    snapshot.insert(QStringLiteral("connectionState"), getConnectionStateName());
    snapshot.insert(QStringLiteral("lastHeartbeatMs"), m_lastHeartbeatMs);
    snapshot.insert(QStringLiteral("lastControlMessageMs"), m_lastControlMessageMs);
    snapshot.insert(QStringLiteral("lastStateSampleMs"), m_lastStateSampleMs);
    snapshot.insert(QStringLiteral("routingErrors"), m_routingErrorCount);
    snapshot.insert(QStringLiteral("datasourceErrors"), m_datasourceErrorCount);
    snapshot.insert(QStringLiteral("ackSentCount"), m_ackCount);
    snapshot.insert(QStringLiteral("ackConfirmedCount"), m_ackConfirmedCount);
    snapshot.insert(QStringLiteral("ackTimeoutCount"), m_ackTimeoutCount);
    snapshot.insert(QStringLiteral("resyncRequestCount"), m_resyncRequestCount);
    snapshot.insert(QStringLiteral("reconnectCount"), m_reconnectCount);
    snapshot.insert(QStringLiteral("droppedReliableCount"), m_droppedReliableCount);
    snapshot.insert(QStringLiteral("receivedControlCount"), m_receivedControlCount);
    snapshot.insert(QStringLiteral("receivedSampleCount"), m_receivedSampleCount);
    snapshot.insert(QStringLiteral("routingChannelCount"), m_routingChannels.size());
    snapshot.insert(QStringLiteral("subscriptionSourceCount"), m_subscriptionSources.size());
    snapshot.insert(QStringLiteral("activePollingPlanCount"), activePollingPlanCount());
    snapshot.insert(QStringLiteral("globalPollingKeyCount"), m_globalPollingKeyCount);
    snapshot.insert(QStringLiteral("pendingAckCount"), m_inflightReliableMessages.size());
    snapshot.insert(QStringLiteral("confirmedWindowCount"), m_confirmedOutboundWindow.size());
    snapshot.insert(QStringLiteral("queuedOutboundControlCount"), m_outboundQueue.size());

    if (snapshot != m_healthSnapshot) {
        m_healthSnapshot = snapshot;
        emit healthSnapshotChanged(m_healthSnapshot);
    } else {
        m_healthSnapshot = snapshot;
    }
}

int CommunicationHub::activePollingPlanCount() const
{
    return m_hasActivePollingPlan ? 1 : 0;
}

bool CommunicationHub::hasSubscriptionSource(const QString& sourceId) const
{
    for (const SubscriptionSource* source : m_subscriptionSources) {
        if (source && source->getSourceId() == sourceId) {
            return true;
        }
    }
    return false;
}
