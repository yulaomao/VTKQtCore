#pragma once

#include "ILogicGateway.h"

#include <QMap>

class LogicRuntime;
class RedisGateway;

class LocalLogicGateway : public ILogicGateway
{
    Q_OBJECT

public:
    explicit LocalLogicGateway(LogicRuntime* runtime,
                               RedisGateway* redisGateway = nullptr,
                               QObject* parent = nullptr);
    ~LocalLogicGateway() override = default;

    bool sendAction(const UiAction& action) override;
    int subscribeNotification(std::function<void(const LogicNotification&)> handler) override;
    void unsubscribeNotification(int subscriptionId) override;

    ConnectionState getConnectionState() const override;
    void requestResync(const QString& reason) override;

private slots:
    void onRuntimeNotification(const LogicNotification& notification);
    void onRedisConnectionStateChanged(int state);

private:
    void updateConnectionState(ConnectionState state);

    LogicRuntime* m_runtime = nullptr;
    RedisGateway* m_redisGateway = nullptr;
    ConnectionState m_connectionState = Connected;
    int m_nextSubscriptionId = 1;
    QMap<int, std::function<void(const LogicNotification&)>> m_subscribers;
};