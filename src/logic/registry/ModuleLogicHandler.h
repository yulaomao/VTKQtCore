#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include "contracts/ModuleInvoke.h"
#include "contracts/UiAction.h"
#include "contracts/LogicNotification.h"
#include "communication/datasource/StateSample.h"

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

    // The connection (and therefore DB) this module should use by default for
    // direct Redis reads/writes and publishes.  Set during initialisation from
    // the dispatch config.  Returns an empty string when not configured.
    void    setDefaultConnectionId(const QString& connectionId);
    QString getDefaultConnectionId() const;

    virtual void handleAction(const UiAction& action) = 0;
    virtual ModuleInvokeResult handleModuleInvoke(const ModuleInvokeRequest& request)
    {
        Q_UNUSED(request);
        return ModuleInvokeResult::failure(
            QStringLiteral("invoke_not_supported"),
            QStringLiteral("Module '%1' does not support internal invocation").arg(m_moduleId));
    }

    // ---------------------------------------------------------------------------
    // Data dispatch — called by RedisDataCenter via LogicRuntime.
    //
    // Polling data is delivered as one aggregated StateSample per module per poll
    // round via handleStateSample(). The sample data always carries a
    // QVariantMap under "values".
    //
    // handleSubscription(): called when a pub/sub message arrives on 'channel'.
    // Default implementation wraps the payload into a StateSample and forwards
    // to handleStateSample() so that existing subclasses continue to work
    // without any changes.
    // ---------------------------------------------------------------------------
    virtual void handleSubscription(const QString& channel, const QVariantMap& payload);

    // Polling data and subscription data ultimately converge here.
    virtual void handleStateSample(const StateSample& sample)
    {
        Q_UNUSED(sample);
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
    QVariant readRedisHashValue(const QString& hashKey, const QString& field);
    QString readRedisHashStringValue(const QString& hashKey, const QString& field);
    QVariantMap readRedisHashJsonValue(const QString& hashKey, const QString& field);
    QVariant readRedisHashValue(const QStringList& path);
    QString readRedisHashStringValue(const QStringList& path);
    QVariantMap readRedisHashJsonValue(const QStringList& path);
    bool writeRedisValue(const QString& key, const QVariant& value);
    bool writeRedisJsonValue(const QString& key, const QVariantMap& value);
    bool writeRedisHashValue(const QStringList& path, const QVariant& value);
    bool writeRedisHashJsonValue(const QStringList& path, const QVariantMap& value);
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
    QString m_defaultConnectionId;
    SceneGraph* m_sceneGraph = nullptr;
    IRedisCommandAccess* m_redisCommandAccess = nullptr;
    IModuleInvoker* m_moduleInvoker = nullptr;
};
