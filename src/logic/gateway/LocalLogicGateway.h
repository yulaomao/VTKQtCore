#pragma once

#include "ILogicGateway.h"

#include <QMap>

class LogicRuntime;
class CommunicationHub;
class RedisGateway;

class LocalLogicGateway : public ILogicGateway
{
    Q_OBJECT

public:
    explicit LocalLogicGateway(LogicRuntime* runtime,
                               CommunicationHub* communicationHub = nullptr,
                               RedisGateway* redisGateway = nullptr,
                               QObject* parent = nullptr);
    ~LocalLogicGateway() override = default;

    void sendAction(const UiAction& action) override;
    int subscribeNotification(std::function<void(const LogicNotification&)> handler) override;
    void unsubscribeNotification(int subscriptionId) override;

    ConnectionState getConnectionState() const override;
    void requestResync(const QString& reason) override;

private slots:
    void onRuntimeNotification(const LogicNotification& notification);
    void onRedisConnectionStateChanged(int state);

private:
    void dispatchNotification(const LogicNotification& notification);
    LogicNotification createRejectedActionNotification(const UiAction& action,
                                                      const QString& errorCode,
                                                      const QString& message,
                                                      const QString& suggestedAction) const;
    void updateConnectionState(ConnectionState state);

    LogicRuntime* m_runtime = nullptr;
    CommunicationHub* m_communicationHub = nullptr;
    RedisGateway* m_redisGateway = nullptr;
    ConnectionState m_connectionState = Connected;
    int m_nextSubscriptionId = 1;
    QMap<int, std::function<void(const LogicNotification&)>> m_subscribers;
};