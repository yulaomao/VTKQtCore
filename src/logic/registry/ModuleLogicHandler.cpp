#include "ModuleLogicHandler.h"

#include "communication/hub/IRedisCommandAccess.h"
#include "communication/datasource/StateSample.h"
#include "ModuleUiEvent.h"
#include "logic/runtime/IModuleInvoker.h"

#include <QDateTime>

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

void ModuleLogicHandler::setDefaultConnectionId(const QString& connectionId)
{
    m_defaultConnectionId = connectionId;
}

QString ModuleLogicHandler::getDefaultConnectionId() const
{
    return m_defaultConnectionId;
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

// ---------------------------------------------------------------------------
// Data dispatch — default implementations (backward-compat shims)
// ---------------------------------------------------------------------------
// Subclasses can override handlePollData() / handleSubscription() directly.
// If they don't, the defaults below call handleStateSample() with a StateSample
// that matches the format produced by the legacy DefaultGlobalPollingSampleParser:
//   data = { "key": <redis_key>, "value": <normalized_value> }
// This lets existing module implementations continue to work unchanged.

void ModuleLogicHandler::handlePollData(const QString& key, const QVariant& value)
{
    QVariantMap data;
    data.insert(QStringLiteral("key"), key);
    data.insert(QStringLiteral("value"), value);

    StateSample sample;
    // sampleId is intentionally left empty for the backward-compat shim:
    // existing module code does not rely on it, and generating UUIDs at
    // 60 Hz would be wasteful.
    sample.sourceId     = QStringLiteral("poll");
    sample.module       = m_moduleId;
    sample.sampleType   = key;
    sample.timestampMs  = QDateTime::currentMSecsSinceEpoch();
    sample.data         = data;

    handleStateSample(sample);
}

void ModuleLogicHandler::handleSubscription(const QString& channel, const QVariantMap& payload)
{
    QVariantMap data = payload;
    data.insert(QStringLiteral("channel"), channel);

    StateSample sample;
    sample.sourceId     = QStringLiteral("subscription");
    sample.module       = m_moduleId;
    sample.sampleType   = QStringLiteral("subscription");
    sample.timestampMs  = QDateTime::currentMSecsSinceEpoch();
    sample.data         = data;

    handleStateSample(sample);
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

bool ModuleLogicHandler::forwardModuleUiEventAction(const UiAction& action,
                                                    const QString& sourceModule)
{
    QString eventName;
    if (!ModuleUiEvent::isAction(action, &eventName)) {
        return false;
    }

    const QString normalizedSourceModule = sourceModule.trimmed().isEmpty()
        ? action.module.trimmed()
        : sourceModule.trimmed();
    emitModuleUiEvent(eventName,
                      action.payload,
                      normalizedSourceModule,
                      action.actionId);
    return true;
}

void ModuleLogicHandler::emitModuleUiEvent(const QString& eventName,
                                           const QVariantMap& payload,
                                           const QString& sourceModule,
                                           const QString& sourceActionId,
                                           LogicNotification::TargetScope scope,
                                           const QStringList& targetModules)
{
    const QString normalizedEventName = eventName.trimmed();
    if (normalizedEventName.isEmpty()) {
        return;
    }

    const QString normalizedSourceModule = sourceModule.trimmed().isEmpty()
        ? m_moduleId
        : sourceModule.trimmed();
    LogicNotification notification = LogicNotification::create(
        LogicNotification::CustomEvent,
        scope,
        ModuleUiEvent::createNotificationPayload(normalizedEventName,
                                                 normalizedSourceModule,
                                                 payload));
    if (scope == LogicNotification::ModuleList) {
        notification.targetModules = targetModules.isEmpty()
            ? QStringList{m_moduleId}
            : targetModules;
    }
    notification.setSourceActionId(sourceActionId);
    emit logicNotification(notification);
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
