#pragma once

#include "ILogicGateway.h"

#include <QMap>

class LogicRuntime;

class LocalLogicGateway : public ILogicGateway
{
    Q_OBJECT

public:
    explicit LocalLogicGateway(LogicRuntime* runtime,
                               QObject* parent = nullptr);
    ~LocalLogicGateway() override = default;

    void sendAction(const UiAction& action) override;
    int subscribeNotification(std::function<void(const LogicNotification&)> handler) override;
    void unsubscribeNotification(int subscriptionId) override;

    ConnectionState getConnectionState() const override;
    void requestResync(const QString& reason) override;

private slots:
    void onRuntimeNotification(const LogicNotification& notification);

private:
    LogicRuntime* m_runtime = nullptr;
    ConnectionState m_connectionState = Connected;
    int m_nextSubscriptionId = 1;
    QMap<int, std::function<void(const LogicNotification&)>> m_subscribers;
};
