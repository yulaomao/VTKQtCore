#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QByteArray>
#include <QSet>

#include <atomic>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>

struct redisContext;
class QTimer;

class RedisGateway : public QObject
{
    Q_OBJECT

public:
    enum ConnectionState {
        Connected,
        Reconnecting,
        Disconnected
    };
    Q_ENUM(ConnectionState)

    explicit RedisGateway(QObject* parent = nullptr);
    ~RedisGateway() override;

    void connectToServer(const QString& host, int port);
    // Select a Redis database on the command connection.  Must be called after
    // connectToServer().  One connection stays on one DB for its lifetime.
    bool selectDb(int db);
    void disconnect();
    ConnectionState getConnectionState() const;
    bool waitForConnected(int timeoutMs = 2000);
    QString getHost() const;
    int getPort() const;

    // Selects the Redis database on the command connection.
    // Returns false and emits errorOccurred() if the command fails.
    // Note: the subscriber connection uses a separate context; subscriptions in
    // Redis are server-scoped (not DB-scoped), so SELECT has no effect there.
    bool selectDb(int db);

    void subscribe(const QString& channel);
    void unsubscribe(const QString& channel);
    bool publish(const QString& channel, const QByteArray& message);

    QVariant readKey(const QString& key);
    bool writeKey(const QString& key, const QVariant& value);
    bool writeJson(const QString& key, const QVariantMap& value);

    QString readString(const QString& key);
    QVariantMap readJson(const QString& key);

signals:
    void connectionStateChanged(RedisGateway::ConnectionState state);
    void messageReceived(const QString& channel, const QByteArray& message);
    void errorOccurred(const QString& errorMessage);

private slots:
    void onReconnectTimeout();

private:
    enum class SubscriptionCommandType {
        Subscribe,
        Unsubscribe
    };

    struct SubscriptionCommand {
        SubscriptionCommandType type = SubscriptionCommandType::Subscribe;
        QString channel;
    };

    void setConnectionState(ConnectionState state);
    void attemptConnection();
    redisContext* createContext(QString* errorMessage) const;
    void clearCommandContextLocked();
    void clearPendingSubscriptionCommands();
    void stopSubscriberWorker();
    void handleConnectionFailure(const QString& reason);
    void queueSubscriptionCommand(SubscriptionCommandType type, const QString& channel);
    void subscriberLoop();
    bool processInitialSubscriptions(redisContext* context);
    bool processPendingSubscriptionCommands(redisContext* context);
    bool appendSubscriptionCommand(redisContext* context, const SubscriptionCommand& command);
    void dispatchSubscriberReply(void* replyObject);
    bool executeSet(redisContext* context, const QString& key, const QByteArray& value,
                    QString* errorMessage) const;
    QVariant executeGet(redisContext* context, const QString& key, QString* errorMessage) const;

    ConnectionState m_connectionState = Disconnected;
    QString m_host;
    int m_port = 0;
    mutable std::mutex m_commandMutex;
    std::mutex m_subscriptionMutex;
    redisContext* m_commandContext = nullptr;
    redisContext* m_subscriberContext = nullptr;
    QTimer* m_reconnectTimer = nullptr;
    QSet<QString> m_subscribedChannels;
    std::deque<SubscriptionCommand> m_pendingSubscriptionCommands;
    std::thread m_subscriberThread;
    std::atomic_bool m_manualDisconnect{false};
    std::atomic_bool m_shutdownRequested{false};
    std::atomic_bool m_stopSubscriber{false};
    int m_reconnectDelayMs = 1000;
    int m_connectTimeoutMs = 2000;
    int m_subscriberPollIntervalMs = 200;
};
