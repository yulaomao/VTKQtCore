#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

#include "RedisConnectionConfig.h"

class RedisGateway;
class RedisPollingWorker;
class QThread;
class QTimer;

// ---------------------------------------------------------------------------
// RedisConnectionWorker
//
// Owns all resources for one Redis connection described by RedisConnectionConfig:
//
//   • Polling:       a dedicated QThread + RedisPollingWorker that fires HGET
//                    for all configured logical polling keys at 'pollIntervalMs' intervals.
//
//   • Subscription:  a RedisGateway that maintains a subscriber thread and
//                    emits messageReceived() for every incoming pub/sub message.
//
// Both sources report their results on the main (owning) thread via
// Qt queued signal–slot connections.
//
// Signals
// -------
//   pollBatchReady(connectionId, values)
//       Emitted when the hash poll completes.  'values' is a raw logicalKey→QVariant
//       map; values may be null QVariants when a field has no data in Redis.
//
//   subscriptionReceived(connectionId, module, channel, payload)
//       Emitted when a pub/sub message arrives on a subscribed channel.
//       'module' is the module name from the config (may be "global").
//       'payload' is the JSON-decoded message body; a raw string payload is
//       returned as {"_raw": "<string>"} when JSON decoding fails.
// ---------------------------------------------------------------------------
class RedisConnectionWorker : public QObject
{
    Q_OBJECT

public:
    explicit RedisConnectionWorker(const RedisConnectionConfig& config,
                                   QObject* parent = nullptr);
    ~RedisConnectionWorker() override;

    QString                    connectionId() const;
    const RedisConnectionConfig& config()    const;

    void start();
    void stop();
    bool isRunning() const;

signals:
    // Raw hash poll result — one entry per configured logical key (null = field absent).
    void pollBatchReady(const QString& connectionId, const QVariantMap& values);

    // Decoded pub/sub message for the given module and channel.
    void subscriptionReceived(const QString& connectionId,
                               const QString& module,
                               const QString& channel,
                               const QVariantMap& payload);

    // Internal: cross-thread trigger to start a poll on the worker thread.
    void requestPoll();

private slots:
    void onPollResult(const QVariantMap& values);
    void onGatewayMessage(const QString& channel, const QByteArray& rawMessage);
    void onPollTimerTick();

private:
    RedisConnectionConfig  m_config;
    bool                   m_running       = false;
    RedisGateway*          m_gateway       = nullptr;
    RedisPollingWorker*    m_pollWorker    = nullptr;
    QThread*               m_pollThread   = nullptr;
    QTimer*      m_pollTimer    = nullptr;
    QStringList  m_allPollingKeys;
};
