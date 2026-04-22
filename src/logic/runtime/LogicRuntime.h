#pragma once

#include <QObject>
#include <QMap>

#include "communication/datasource/StateSample.h"
#include "contracts/ModuleInvoke.h"
#include "contracts/UiAction.h"
#include "contracts/LogicNotification.h"
#include "IModuleInvoker.h"

class SceneGraph;
class ActiveModuleState;
class ModuleLogicRegistry;
class ModuleLogicHandler;
class GlobalPollingSampleParser;
class IRedisCommandAccess;

class LogicRuntime : public QObject, public IModuleInvoker
{
    Q_OBJECT

public:
    explicit LogicRuntime(QObject* parent = nullptr);

    SceneGraph* getSceneGraph() const;
    ActiveModuleState* getActiveModuleState() const;
    ModuleLogicRegistry* getModuleLogicRegistry() const;
    void setGlobalPollingSampleParser(GlobalPollingSampleParser* parser);
    void setRedisCommandAccess(IRedisCommandAccess* redisCommandAccess);
    bool hasRedisCommandAccess() const;
    QVariant readRedisValue(const QString& key);
    QString readRedisStringValue(const QString& key);
    QVariantMap readRedisJsonValue(const QString& key);
    bool writeRedisValue(const QString& key, const QVariant& value);
    bool writeRedisJsonValue(const QString& key, const QVariantMap& value);
    bool publishRedisMessage(const QString& channel, const QByteArray& message);
    bool publishRedisJsonMessage(const QString& channel, const QVariantMap& payload);
    ModuleInvokeResult invokeModule(const ModuleInvokeRequest& request) override;

    void registerModuleHandler(ModuleLogicHandler* handler);

public slots:
    void onActionReceived(const UiAction& action);
    void onControlMessageReceived(const QString& module, const QVariantMap& payload);
    void onServerCommandReceived(const QString& commandType, const QVariantMap& payload);
    void onStateSampleReceived(const StateSample& sample);
    void onCommunicationError(const QString& source, const QString& errorMessage);
    void onCommunicationIssue(const QString& source, const QString& severity,
                              const QString& errorCode, const QString& errorMessage,
                              const QVariantMap& context);
    void onCommunicationHealthChanged(const QVariantMap& healthSnapshot);
    void onConnectionStateChanged(const QString& state);
    void requestResync(const QString& reason);

    // ---------------------------------------------------------------------------
    // Data dispatch from RedisDataCenter
    // ---------------------------------------------------------------------------
    // Called once per Redis key per poll cycle.  'module' may be "global" to
    // broadcast the key/value to ALL registered module handlers.
    void onModulePollKey(const QString& module, const QString& key, const QVariant& value);

    // Called when a pub/sub message arrives for a module.  'module' may be
    // "global" to broadcast to ALL registered module handlers.
    void onModuleSubscription(const QString& module, const QString& channel,
                               const QVariantMap& payload);

signals:
    void logicNotification(const LogicNotification& notification);

private:
    bool acceptIncomingSequence(const QString& streamKey, const QVariantMap& payload,
                                const QString& actionDescription);
    void switchToModule(const QString& targetModule, const QString& sourceActionId);
    void routeToModuleHandler(const UiAction& action);

    SceneGraph* m_sceneGraph;
    ActiveModuleState* m_activeModuleState;
    ModuleLogicRegistry* m_moduleLogicRegistry;
    GlobalPollingSampleParser* m_globalPollingSampleParser = nullptr;
    IRedisCommandAccess* m_redisCommandAccess = nullptr;
    QMap<QString, qint64> m_lastInboundSeqByStream;
};
