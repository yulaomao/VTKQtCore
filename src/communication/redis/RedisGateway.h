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
#include <vector>

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
    void disconnect();
    ConnectionState getConnectionState() const;
    bool waitForConnected(int timeoutMs = 2000);

    void subscribe(const QString& channel);
    void unsubscribe(const QString& channel);
    void publish(const QString& channel, const QByteArray& message);

    QVariant readKey(const QString& key);
    void asyncReadKey(const QString& key);

    QString readString(const QString& key);
    QVariantMap readJson(const QString& key);

signals:
    void connectionStateChanged(RedisGateway::ConnectionState state);
    void messageReceived(const QString& channel, const QByteArray& message);
    void keyValueReceived(const QString& key, const QVariant& value);
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
    QVariant executeGet(redisContext* context, const QString& key, QString* errorMessage) const;
    void cleanupAsyncWorkers(bool waitForAll);

    ConnectionState m_connectionState = Disconnected;
    QString m_host;
    int m_port = 0;
    mutable std::mutex m_commandMutex;
    std::mutex m_subscriptionMutex;
    std::mutex m_asyncMutex;
    redisContext* m_commandContext = nullptr;
    redisContext* m_subscriberContext = nullptr;
    QTimer* m_reconnectTimer = nullptr;
    QSet<QString> m_subscribedChannels;
    std::deque<SubscriptionCommand> m_pendingSubscriptionCommands;
    struct AsyncWorker {
        std::shared_ptr<std::atomic_bool> done;
        std::thread thread;
    };
    std::vector<AsyncWorker> m_asyncWorkers;
    std::thread m_subscriberThread;
    std::atomic_bool m_manualDisconnect{false};
    std::atomic_bool m_shutdownRequested{false};
    std::atomic_bool m_stopSubscriber{false};
    int m_reconnectDelayMs = 1000;
    int m_connectTimeoutMs = 2000;
    int m_subscriberPollIntervalMs = 200;
};
