#include "LocalLogicGateway.h"

#include "communication/hub/CommunicationHub.h"
#include "logic/runtime/LogicRuntime.h"
#include "communication/redis/RedisGateway.h"

namespace {

LogicNotification createGatewayWarning(const QString& errorCode,
                                       const QString& message,
                                       const QString& suggestedAction,
                                       const QString& sourceActionId = QString())
{
    LogicNotification notification = LogicNotification::create(
        LogicNotification::ErrorOccurred,
        LogicNotification::Shell,
        {{QStringLiteral("errorCode"), errorCode},
         {QStringLiteral("message"), message},
         {QStringLiteral("recoverable"), true},
         {QStringLiteral("suggestedAction"), suggestedAction}});
    notification.setLevel(LogicNotification::Warning);
    notification.setSourceActionId(sourceActionId);
    return notification;
}

}

LocalLogicGateway::LocalLogicGateway(LogicRuntime* runtime,
                                     CommunicationHub* communicationHub,
                                     RedisGateway* redisGateway,
                                     QObject* parent)
    : ILogicGateway(parent)
    , m_runtime(runtime)
    , m_communicationHub(communicationHub)
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

void LocalLogicGateway::sendAction(const UiAction& action)
{
    if (m_connectionState == Disconnected) {
        onRuntimeNotification(createGatewayWarning(
            QStringLiteral("GATEWAY_DISCONNECTED"),
            QStringLiteral("当前连接已断开，无法发送操作请求。"),
            QStringLiteral("等待连接恢复后重试，或执行重新同步。"),
            action.actionId));
        return;
    }

    if (m_communicationHub) {
        m_communicationHub->sendActionRequest(action, true);
        return;
    }

    if (!m_runtime) {
        onRuntimeNotification(createGatewayWarning(
            QStringLiteral("GATEWAY_RUNTIME_UNAVAILABLE"),
            QStringLiteral("逻辑运行时未就绪，无法处理操作请求。"),
            QStringLiteral("检查应用初始化流程，确认 LogicRuntime 已创建。"),
            action.actionId));
        return;
    }

    m_runtime->onActionReceived(action);
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
    if (m_communicationHub) {
        m_communicationHub->sendResyncRequest(reason, true);
        return;
    }

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