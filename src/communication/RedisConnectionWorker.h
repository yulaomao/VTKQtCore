#pragma once

#include <QObject>
#include <QMap>
#include <QString>
#include <QVariant>
#include <QByteArray>
#include <QJsonDocument>

#include <atomic>
#include <mutex>
#include <thread>

#include "RedisConnectionConfig.h"
#include "communication/hub/IRedisCommandAccess.h"

struct redisContext;
class QTimer;

// Runs on its own QThread.
// Manages one Redis connection that:
//   - Polls a set of keys with MGET at a configurable interval.
//   - Subscribes to Pub/Sub channels.
//   - Provides thread-safe GET/SET/PUBLISH command access.
class RedisConnectionWorker : public QObject, public IRedisCommandAccess
{
    Q_OBJECT

public:
    explicit RedisConnectionWorker(const RedisConnectionConfig& config,
                                   QObject* parent = nullptr);
    ~RedisConnectionWorker() override;

    // IRedisCommandAccess – thread-safe, callable from any thread.
    bool isAvailable() const override;
    QVariant readValue(const QString& key) override;
    QString readStringValue(const QString& key) override;
    QVariantMap readJsonValue(const QString& key) override;
    bool writeValue(const QString& key, const QVariant& value) override;
    bool writeJsonValue(const QString& key, const QVariantMap& value) override;
    bool publishMessage(const QString& channel, const QByteArray& message) override;
    bool publishJsonMessage(const QString& channel, const QVariantMap& payload) override;

public slots:
    void start();
    void stop();

signals:
    // Emitted once per Redis key per polling cycle.
    void pollKeyResult(const QString& connectionId, const QString& module,
                       const QString& key, const QVariant& value);

    // Emitted for each Pub/Sub message received.
    void subscriptionMessage(const QString& connectionId, const QString& module,
                              const QString& channel, const QByteArray& rawData);

private slots:
    void onPollTimer();

private:
    // Command context helpers (all must be called while holding m_cmdMutex, or
    // from the worker thread before the subscriber thread starts).
    bool ensureCommandConnectedLocked();
    void closeCommandContextLocked();
    redisContext* createHiredisContext(int connectTimeoutMs, QString* errMsg) const;
    bool selectDbLocked(redisContext* ctx);

    // Redis reply helpers.
    static QVariant replyToVariant(void* replyObject);
    static QByteArray variantToBytes(const QVariant& value);

    // Subscriber thread.
    void subscriberLoop();
    void stopSubscriber();

    RedisConnectionConfig m_config;

    // Command + polling connection – guarded by m_cmdMutex.
    mutable std::mutex m_cmdMutex;
    redisContext*      m_cmdContext = nullptr;

    // Subscriber connection – lives entirely on m_subThread.
    std::thread            m_subThread;
    std::atomic_bool       m_stopSubscriber{false};

    QTimer* m_pollTimer = nullptr;
    bool    m_started   = false;

    static constexpr int kConnectTimeoutMs = 2000;
};
