#include "LocalLogicGateway.h"

#include "logic/runtime/LogicRuntime.h"
#include "communication/redis/RedisGateway.h"

LocalLogicGateway::LocalLogicGateway(LogicRuntime* runtime,
                                     RedisGateway* redisGateway,
                                     QObject* parent)
    : ILogicGateway(parent)
    , m_runtime(runtime)
    , m_redisGateway(redisGateway)
{
    if (m_runtime) {
        connect(m_runtime, &LogicRuntime::logicNotification,
                this, &LocalLogicGateway::onRuntimeNotification);
    }

    if (m_redisGateway) {
        connect(m_redisGateway, &RedisGateway::connectionStateChanged,
                this, [this](RedisGateway::ConnectionState state) {
                    onRedisConnectionStateChanged(static_cast<int>(state));
                });
        onRedisConnectionStateChanged(static_cast<int>(m_redisGateway->getConnectionState()));
    }
}

bool LocalLogicGateway::sendAction(const UiAction& action)
{
    if (!m_runtime || m_connectionState == Disconnected) {
        return false;
    }

    m_runtime->onActionReceived(action);
    return true;
}

int LocalLogicGateway::subscribeNotification(std::function<void(const LogicNotification&)> handler)
{
    if (!handler) {
        return -1;
    }

    const int subscriptionId = m_nextSubscriptionId++;
    m_subscribers.insert(subscriptionId, std::move(handler));
    return subscriptionId;
}

void LocalLogicGateway::unsubscribeNotification(int subscriptionId)
{
    m_subscribers.remove(subscriptionId);
}

ILogicGateway::ConnectionState LocalLogicGateway::getConnectionState() const
{
    return m_connectionState;
}

void LocalLogicGateway::requestResync(const QString& reason)
{
    if (!m_runtime) {
        return;
    }

    m_runtime->requestResync(reason);
}

void LocalLogicGateway::onRuntimeNotification(const LogicNotification& notification)
{
    emit notificationReceived(notification);

    for (auto it = m_subscribers.cbegin(); it != m_subscribers.cend(); ++it) {
        it.value()(notification);
    }
}

void LocalLogicGateway::onRedisConnectionStateChanged(int state)
{
    if (!m_redisGateway) {
        updateConnectionState(Connected);
        return;
    }

    switch (static_cast<RedisGateway::ConnectionState>(state)) {
    case RedisGateway::Connected:
        updateConnectionState(Connected);
        break;
    case RedisGateway::Reconnecting:
        updateConnectionState(Degraded);
        break;
    case RedisGateway::Disconnected:
    default:
        updateConnectionState(Disconnected);
        break;
    }
}

void LocalLogicGateway::updateConnectionState(ConnectionState state)
{
    m_connectionState = state;
}