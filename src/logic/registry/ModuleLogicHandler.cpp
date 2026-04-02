#include "ModuleLogicHandler.h"

#include "communication/hub/IRedisCommandAccess.h"
#include "logic/runtime/IModuleInvoker.h"

ModuleLogicHandler::ModuleLogicHandler(const QString& moduleId, QObject* parent)
    : QObject(parent)
    , m_moduleId(moduleId)
{
}

QString ModuleLogicHandler::getModuleId() const
{
    return m_moduleId;
}

void ModuleLogicHandler::setSceneGraph(SceneGraph* scene)
{
    m_sceneGraph = scene;
}

SceneGraph* ModuleLogicHandler::getSceneGraph() const
{
    return m_sceneGraph;
}

void ModuleLogicHandler::setRedisCommandAccess(IRedisCommandAccess* redisCommandAccess)
{
    m_redisCommandAccess = redisCommandAccess;
}

void ModuleLogicHandler::setModuleInvoker(IModuleInvoker* moduleInvoker)
{
    m_moduleInvoker = moduleInvoker;
}

bool ModuleLogicHandler::hasRedisCommandAccess() const
{
    return m_redisCommandAccess && m_redisCommandAccess->isAvailable();
}

QVariant ModuleLogicHandler::readRedisValue(const QString& key)
{
    return m_redisCommandAccess ? m_redisCommandAccess->readValue(key) : QVariant();
}

QString ModuleLogicHandler::readRedisStringValue(const QString& key)
{
    return m_redisCommandAccess ? m_redisCommandAccess->readStringValue(key) : QString();
}

QVariantMap ModuleLogicHandler::readRedisJsonValue(const QString& key)
{
    return m_redisCommandAccess ? m_redisCommandAccess->readJsonValue(key) : QVariantMap();
}

bool ModuleLogicHandler::writeRedisValue(const QString& key, const QVariant& value)
{
    return m_redisCommandAccess && m_redisCommandAccess->writeValue(key, value);
}

bool ModuleLogicHandler::writeRedisJsonValue(const QString& key, const QVariantMap& value)
{
    return m_redisCommandAccess && m_redisCommandAccess->writeJsonValue(key, value);
}

bool ModuleLogicHandler::publishRedisMessage(const QString& channel, const QByteArray& message)
{
    return m_redisCommandAccess && m_redisCommandAccess->publishMessage(channel, message);
}

bool ModuleLogicHandler::publishRedisJsonMessage(const QString& channel, const QVariantMap& payload)
{
    return m_redisCommandAccess && m_redisCommandAccess->publishJsonMessage(channel, payload);
}

ModuleInvokeResult ModuleLogicHandler::invokeModule(const QString& targetModule,
                                                    const QString& method,
                                                    const QVariantMap& payload)
{
    if (!m_moduleInvoker) {
        return ModuleInvokeResult::failure(
            QStringLiteral("module_invoker_unavailable"),
            QStringLiteral("Internal module invoker is not configured"),
            {{QStringLiteral("sourceModule"), m_moduleId},
             {QStringLiteral("targetModule"), targetModule},
             {QStringLiteral("method"), method}});
    }

    return m_moduleInvoker->invokeModule(
        ModuleInvokeRequest::create(m_moduleId, targetModule, method, payload));
}

void ModuleLogicHandler::emitInvokeFailureNotification(const ModuleInvokeResult& result,
                                                       const QString& targetModule,
                                                       const QString& sourceActionId)
{
    if (result.ok) {
        return;
    }

    LogicNotification notification = LogicNotification::create(
        LogicNotification::ErrorOccurred,
        LogicNotification::CurrentModule,
        {{QStringLiteral("errorCode"), result.errorCode},
         {QStringLiteral("message"), result.message},
         {QStringLiteral("targetModule"), targetModule},
         {QStringLiteral("invokePayload"), result.payload}});
    notification.setSourceActionId(sourceActionId);
    notification.setLevel(LogicNotification::Warning);
    emit logicNotification(notification);
}
