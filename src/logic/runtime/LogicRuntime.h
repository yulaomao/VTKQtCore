#pragma once

#include <QObject>
#include <QMap>

#include "contracts/ModuleInvoke.h"
#include "contracts/UiAction.h"
#include "contracts/LogicNotification.h"
#include "IModuleInvoker.h"

class SceneGraph;
class ActiveModuleState;
class ModuleLogicRegistry;
class ModuleLogicHandler;
class IRedisCommandAccess;

class LogicRuntime : public QObject, public IModuleInvoker
{
    Q_OBJECT

public:
    explicit LogicRuntime(QObject* parent = nullptr);

    SceneGraph* getSceneGraph() const;
    ActiveModuleState* getActiveModuleState() const;
    ModuleLogicRegistry* getModuleLogicRegistry() const;

    // Optional Redis command access used by module handlers for direct GET/SET/PUBLISH.
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
    IRedisCommandAccess* m_redisCommandAccess = nullptr;
    QMap<QString, qint64> m_lastInboundSeqByStream;
};
