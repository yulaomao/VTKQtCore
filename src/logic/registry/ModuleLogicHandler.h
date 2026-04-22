#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include "contracts/ModuleInvoke.h"
#include "contracts/UiAction.h"
#include "contracts/LogicNotification.h"

#include <QVariant>
#include <QVariantMap>

class IRedisCommandAccess;
class IModuleInvoker;
class SceneGraph;

class ModuleLogicHandler : public QObject
{
    Q_OBJECT

public:
    explicit ModuleLogicHandler(const QString& moduleId, QObject* parent = nullptr);

    QString getModuleId() const;
    void setSceneGraph(SceneGraph* scene);
    SceneGraph* getSceneGraph() const;
    void setRedisCommandAccess(IRedisCommandAccess* redisCommandAccess);
    void setModuleInvoker(IModuleInvoker* moduleInvoker);

    virtual void handleAction(const UiAction& action) = 0;
    virtual ModuleInvokeResult handleModuleInvoke(const ModuleInvokeRequest& request)
    {
        Q_UNUSED(request);
        return ModuleInvokeResult::failure(
            QStringLiteral("invoke_not_supported"),
            QStringLiteral("Module '%1' does not support internal invocation").arg(m_moduleId));
    }

    // Called by MessageDispatchCenter when a polled Redis key has a new value.
    // key   – the Redis key that was polled.
    // value – the raw value returned by Redis (usually a QByteArray containing JSON).
    virtual void handlePollData(const QString& key, const QVariant& value)
    {
        Q_UNUSED(key);
        Q_UNUSED(value);
    }

    // Called by MessageDispatchCenter when a Pub/Sub message arrives on a
    // subscribed channel.
    // channel – the Redis channel name.
    // data    – the message body decoded as a QVariantMap (JSON object).
    virtual void handleSubscribeData(const QString& channel, const QVariantMap& data)
    {
        Q_UNUSED(channel);
        Q_UNUSED(data);
    }

    virtual void onModuleActivated() {}
    virtual void onModuleDeactivated() {}
    virtual void onResync() {}

signals:
    void logicNotification(const LogicNotification& notification);

protected:
    bool hasRedisCommandAccess() const;
    QVariant readRedisValue(const QString& key);
    QString readRedisStringValue(const QString& key);
    QVariantMap readRedisJsonValue(const QString& key);
    bool writeRedisValue(const QString& key, const QVariant& value);
    bool writeRedisJsonValue(const QString& key, const QVariantMap& value);
    bool publishRedisMessage(const QString& channel, const QByteArray& message);
    bool publishRedisJsonMessage(const QString& channel, const QVariantMap& payload);
    ModuleInvokeResult invokeModule(const QString& targetModule,
                                    const QString& method,
                                    const QVariantMap& payload = {});
    bool forwardModuleUiEventAction(const UiAction& action,
                                    const QString& sourceModule = QString());
    void emitModuleUiEvent(const QString& eventName,
                           const QVariantMap& payload = {},
                           const QString& sourceModule = QString(),
                           const QString& sourceActionId = QString(),
                           LogicNotification::TargetScope scope = LogicNotification::ModuleList,
                           const QStringList& targetModules = {});
    void emitInvokeFailureNotification(const ModuleInvokeResult& result,
                                       const QString& targetModule,
                                       const QString& sourceActionId = QString());

private:
    const QString m_moduleId;
    SceneGraph* m_sceneGraph = nullptr;
    IRedisCommandAccess* m_redisCommandAccess = nullptr;
    IModuleInvoker* m_moduleInvoker = nullptr;
};
