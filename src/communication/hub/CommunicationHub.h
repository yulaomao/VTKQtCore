#pragma once

#include <QList>
#include <QMap>
#include <QObject>
#include <QSet>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include "contracts/UiAction.h"
#include "communication/datasource/StateSample.h"
#include "communication/redis/RedisGateway.h"

class MessageRouter;
class PollingSource;
class PollingTask;
class QThread;
class QTimer;
class RedisPollingWorker;
class SubscriptionSource;

class CommunicationHub : public QObject
{
    Q_OBJECT

    struct OutboundControlMessage {
        QString channel;
        QVariantMap payload;
        QString msgId;
        bool requireAck = false;
        int retryCount = 0;
        qint64 firstQueuedMs = 0;
        qint64 lastSentMs = 0;
    };

public:
    explicit CommunicationHub(RedisGateway* gateway, QObject* parent = nullptr);
    ~CommunicationHub() override;

    void initialize();
    void addRoutingChannel(const QString& channel);
    void addSubscriptionSource(SubscriptionSource* source);
    void addPollingTask(PollingTask* task);
    void setOutboundChannels(const QString& controlPublishChannel,
                             const QString& ackChannel);
    bool sendActionRequest(const UiAction& action, bool loopbackToLocal = true);
    bool sendResyncRequest(const QString& reason, bool loopbackToLocal = true);
    void start();
    void stop();
    QString getConnectionStateName() const;
    QVariantMap getHealthSnapshot() const;

signals:
    void controlMessageReceived(const QString& module, const QVariantMap& payload);
    void serverCommandReceived(const QString& commandType, const QVariantMap& payload);
    void stateSampleReceived(const StateSample& sample);
    void communicationError(const QString& source, const QString& errorMessage);
    void communicationIssue(const QString& source, const QString& severity,
                            const QString& errorCode, const QString& errorMessage,
                            const QVariantMap& context);
    void connectionStateChanged(const QString& state);
    void healthSnapshotChanged(const QVariantMap& snapshot);
    void heartbeatReceived();

private slots:
    void onAckReceived(const QVariantMap& payload);
    void onGatewayMessageReceived(const QString& channel, const QByteArray& message);
    void onGatewayConnectionStateChanged(RedisGateway::ConnectionState state);
    void onOutboundRetryTimeout();

private:
    void connectSource(SubscriptionSource* source);
    void activateTransport();
    void deactivateTransport();
    void startPollingTransport(bool blocking = false);
    void stopPollingTransport(bool blocking = false);
    void publishAck(const QString& category, const QVariantMap& payload,
                    const QString& status = QStringLiteral("received"));
    bool publishJson(const QString& channel, const QVariantMap& payload);
    void dispatchOutboundMessage(OutboundControlMessage message);
    void flushOutboundQueue();
    void resendInflightMessages();
    bool shouldAck(const QString& category, const QVariantMap& payload) const;
    bool requiresReliableAck(const QString& channel, const QVariantMap& payload) const;
    bool isSelfOriginated(const QVariantMap& payload) const;
    void emitIssue(const QString& source, const QString& severity,
                   const QString& errorCode, const QString& errorMessage,
                   const QVariantMap& context = {});
    void cleanupIdempotencyWindow(qint64 nowMs);
    void refreshHealthSnapshot();
    int activePollingTaskCount() const;
    bool hasSubscriptionSource(const QString& sourceId) const;
    bool hasPollingTask(const QString& taskId) const;

    RedisGateway* m_redisGateway = nullptr;
    MessageRouter* m_messageRouter = nullptr;
    QVector<SubscriptionSource*> m_subscriptionSources;
    PollingSource* m_pollingSource = nullptr;
    RedisPollingWorker* m_pollingWorker = nullptr;
    QThread* m_pollingThread = nullptr;
    QTimer* m_outboundRetryTimer = nullptr;
    QSet<QString> m_routingChannels;
    QSet<QString> m_pollingTaskIds;
    bool m_started = false;
    bool m_pollingTransportRunning = false;
    RedisGateway::ConnectionState m_lastConnectionState = RedisGateway::Disconnected;
    QString m_controlPublishChannel = QStringLiteral("control.upstream");
    QString m_ackChannel = QStringLiteral("control.ack");
    QString m_clientInstanceId;
    QVariantMap m_healthSnapshot;
    qint64 m_lastHeartbeatMs = 0;
    qint64 m_lastControlMessageMs = 0;
    qint64 m_lastStateSampleMs = 0;
    int m_routingErrorCount = 0;
    int m_datasourceErrorCount = 0;
    int m_ackCount = 0;
    int m_ackConfirmedCount = 0;
    int m_ackTimeoutCount = 0;
    int m_resyncRequestCount = 0;
    int m_reconnectCount = 0;
    int m_droppedReliableCount = 0;
    int m_receivedControlCount = 0;
    int m_receivedSampleCount = 0;
    int m_activePollingTaskCount = 0;
    qint64 m_nextOutboundSeq = 1;
    QList<OutboundControlMessage> m_outboundQueue;
    QMap<QString, OutboundControlMessage> m_inflightReliableMessages;
    QMap<QString, qint64> m_confirmedOutboundWindow;
    int m_outboundRetryLimit = 3;
    int m_outboundAckTimeoutMs = 2000;
    int m_outboundRetryTickMs = 500;
    int m_idempotencyWindowMs = 60000;
};
