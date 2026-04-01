#pragma once

#include <functional>

#include <QObject>
#include <QString>

#include "contracts/UiAction.h"
#include "contracts/LogicNotification.h"

class ILogicGateway : public QObject
{
    Q_OBJECT

public:
    enum ConnectionState {
        Connected,
        Degraded,
        Disconnected
    };

    explicit ILogicGateway(QObject* parent = nullptr) : QObject(parent) {}
    ~ILogicGateway() override = default;

    virtual bool sendAction(const UiAction& action) = 0;

    virtual int  subscribeNotification(
                     std::function<void(const LogicNotification&)> handler) = 0;
    virtual void unsubscribeNotification(int subscriptionId) = 0;

    virtual ConnectionState getConnectionState() const = 0;
    virtual void requestResync(const QString& reason) = 0;

Q_SIGNALS:
    void notificationReceived(const LogicNotification& notification);
};
