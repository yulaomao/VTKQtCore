#include "LocalLogicGateway.h"

#include "logic/runtime/LogicRuntime.h"

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

LocalLogicGateway::LocalLogicGateway(LogicRuntime* runtime, QObject* parent)
    : ILogicGateway(parent)
    , m_runtime(runtime)
{
    if (m_runtime) {
        connect(m_runtime, &LogicRuntime::logicNotification,
                this, &LocalLogicGateway::onRuntimeNotification);
    }
}

void LocalLogicGateway::sendAction(const UiAction& action)
{
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

int LocalLogicGateway::subscribeNotification(
    std::function<void(const LogicNotification&)> handler)
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
    Q_UNUSED(reason)
}

void LocalLogicGateway::onRuntimeNotification(const LogicNotification& notification)
{
    for (auto it = m_subscribers.cbegin(); it != m_subscribers.cend(); ++it) {
        it.value()(notification);
    }
}
