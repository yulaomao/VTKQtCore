#include "LogicRuntime.h"

#include "communication/hub/IRedisCommandAccess.h"
#include "scene/SceneGraph.h"
#include "workflow/ActiveModuleState.h"
#include "registry/ModuleLogicRegistry.h"
#include "registry/ModuleLogicHandler.h"

#include <limits>

namespace {

QString normalizeKey(const QString& value)
{
    QString normalized = value.trimmed().toLower();
    normalized.replace(QLatin1Char('-'), QLatin1Char('_'));
    return normalized;
}

QString actionCommand(const QVariantMap& payload)
{
    return normalizeKey(payload.value(QStringLiteral("command")).toString());
}

bool isSwitchModuleCommand(const QString& command)
{
    return command == QStringLiteral("switch_module") ||
        command == QStringLiteral("request_switch_module");
}

LogicNotification createShellError(const QString& errorCode,
                                   const QString& message,
                                   bool recoverable,
                                   const QString& suggestedAction,
                                   const QVariantMap& extraPayload = {})
{
    QVariantMap payload = extraPayload;
    payload.insert(QStringLiteral("errorCode"), errorCode);
    payload.insert(QStringLiteral("message"), message);
    payload.insert(QStringLiteral("recoverable"), recoverable);
    payload.insert(QStringLiteral("suggestedAction"), suggestedAction);

    LogicNotification notification = LogicNotification::create(
        LogicNotification::ErrorOccurred,
        LogicNotification::Shell,
        payload);
    notification.setLevel(LogicNotification::Warning);
    return notification;
}

QString resolveSwitchTargetModule(const UiAction& action)
{
    const QString targetModule = action.payload.value(QStringLiteral("targetModule")).toString().trimmed();
    if (!targetModule.isEmpty()) {
        return targetModule;
    }

    const QString actionModule = action.module.trimmed();
    if (!actionModule.isEmpty() && actionModule != QStringLiteral("shell")) {
        return actionModule;
    }

    return QString();
}

QString describeAction(const UiAction& action)
{
    const QString command = actionCommand(action.payload);
    return command.isEmpty() ? UiAction::toString(action.actionType) : command;
}

} // namespace

LogicRuntime::LogicRuntime(QObject* parent)
    : QObject(parent)
    , m_sceneGraph(new SceneGraph(this))
    , m_activeModuleState(new ActiveModuleState(this))
    , m_moduleLogicRegistry(new ModuleLogicRegistry(this))
{
}

SceneGraph* LogicRuntime::getSceneGraph() const
{
    return m_sceneGraph;
}

ActiveModuleState* LogicRuntime::getActiveModuleState() const
{
    return m_activeModuleState;
}

ModuleLogicRegistry* LogicRuntime::getModuleLogicRegistry() const
{
    return m_moduleLogicRegistry;
}

void LogicRuntime::setRedisCommandAccess(IRedisCommandAccess* redisCommandAccess)
{
    m_redisCommandAccess = redisCommandAccess;
}

bool LogicRuntime::hasRedisCommandAccess() const
{
    return m_redisCommandAccess && m_redisCommandAccess->isAvailable();
}

QVariant LogicRuntime::readRedisValue(const QString& key)
{
    return m_redisCommandAccess ? m_redisCommandAccess->readValue(key) : QVariant();
}

QString LogicRuntime::readRedisStringValue(const QString& key)
{
    return m_redisCommandAccess ? m_redisCommandAccess->readStringValue(key) : QString();
}

QVariantMap LogicRuntime::readRedisJsonValue(const QString& key)
{
    return m_redisCommandAccess ? m_redisCommandAccess->readJsonValue(key) : QVariantMap();
}

bool LogicRuntime::writeRedisValue(const QString& key, const QVariant& value)
{
    return m_redisCommandAccess && m_redisCommandAccess->writeValue(key, value);
}

bool LogicRuntime::writeRedisJsonValue(const QString& key, const QVariantMap& value)
{
    return m_redisCommandAccess && m_redisCommandAccess->writeJsonValue(key, value);
}

bool LogicRuntime::publishRedisMessage(const QString& channel, const QByteArray& message)
{
    return m_redisCommandAccess && m_redisCommandAccess->publishMessage(channel, message);
}

bool LogicRuntime::publishRedisJsonMessage(const QString& channel, const QVariantMap& payload)
{
    return m_redisCommandAccess && m_redisCommandAccess->publishJsonMessage(channel, payload);
}

void LogicRuntime::registerModuleHandler(ModuleLogicHandler* handler)
{
    if (!handler) {
        return;
    }

    handler->setSceneGraph(m_sceneGraph);
    handler->setRedisCommandAccess(m_redisCommandAccess);
    handler->setModuleInvoker(this);
    m_moduleLogicRegistry->registerHandler(handler);

    connect(handler, &ModuleLogicHandler::logicNotification,
            this, &LogicRuntime::logicNotification);
}

ModuleInvokeResult LogicRuntime::invokeModule(const ModuleInvokeRequest& request)
{
    if (request.targetModule.trimmed().isEmpty()) {
        return ModuleInvokeResult::failure(
            QStringLiteral("target_module_empty"),
            QStringLiteral("Target module is empty"),
            {{QStringLiteral("sourceModule"), request.sourceModule},
             {QStringLiteral("method"), request.method}});
    }

    ModuleLogicHandler* handler = m_moduleLogicRegistry->getHandler(request.targetModule);
    if (!handler) {
        return ModuleInvokeResult::failure(
            QStringLiteral("target_module_unregistered"),
            QStringLiteral("No handler registered for target module '%1'")
                .arg(request.targetModule),
            {{QStringLiteral("sourceModule"), request.sourceModule},
             {QStringLiteral("targetModule"), request.targetModule},
             {QStringLiteral("method"), request.method}});
    }

    return handler->handleModuleInvoke(request);
}

void LogicRuntime::onActionReceived(const UiAction& action)
{
    const QString command = actionCommand(action.payload);
    if (isSwitchModuleCommand(command)) {
        const QString targetModule = resolveSwitchTargetModule(action);
        if (targetModule.isEmpty()) {
            LogicNotification notification = createShellError(
                QStringLiteral("LOGIC_SWITCH_TARGET_EMPTY"),
                QStringLiteral("Module switch command is missing targetModule"),
                true,
                QStringLiteral("Provide payload.targetModule when requesting a shell-level module switch."));
            notification.setSourceActionId(action.actionId);
            notification.setLevel(LogicNotification::Error);
            emit logicNotification(notification);
            return;
        }

        if (!m_moduleLogicRegistry->getHandler(targetModule)) {
            LogicNotification notification = createShellError(
                QStringLiteral("LOGIC_SWITCH_TARGET_UNREGISTERED"),
                QStringLiteral("No module handler registered for switch target '%1'").arg(targetModule),
                true,
                QStringLiteral("Check module registration and the requested targetModule."),
                {{QStringLiteral("targetModule"), targetModule}});
            notification.setSourceActionId(action.actionId);
            notification.setLevel(LogicNotification::Error);
            emit logicNotification(notification);
            return;
        }

        switchToModule(targetModule, action.actionId);
        return;
    }

    routeToModuleHandler(action);
}

void LogicRuntime::switchToModule(const QString& targetModule, const QString& sourceActionId)
{
    const QString oldModule = m_activeModuleState->getCurrentModule();

    if (targetModule.isEmpty() || targetModule == oldModule) {
        return;
    }

    if (!oldModule.isEmpty()) {
        ModuleLogicHandler* oldHandler = m_moduleLogicRegistry->getHandler(oldModule);
        if (oldHandler) {
            oldHandler->onModuleDeactivated();
        }
    }

    m_activeModuleState->setCurrentModule(targetModule);

    ModuleLogicHandler* newHandler = m_moduleLogicRegistry->getHandler(targetModule);
    if (newHandler) {
        newHandler->onModuleActivated();
    }

    LogicNotification notification = LogicNotification::create(
        LogicNotification::ModuleChanged,
        LogicNotification::Shell,
        {{QStringLiteral("newModule"), targetModule},
         {QStringLiteral("oldModule"), oldModule}});
    notification.setSourceActionId(sourceActionId);
    emit logicNotification(notification);

    LogicNotification activeModuleNotification = LogicNotification::create(
        LogicNotification::ActiveModuleChanged,
        LogicNotification::Shell,
        m_activeModuleState->createSnapshot());
    activeModuleNotification.setSourceActionId(sourceActionId);
    emit logicNotification(activeModuleNotification);
}

void LogicRuntime::routeToModuleHandler(const UiAction& action)
{
    QString targetModule = action.payload.value(QStringLiteral("targetModule")).toString().trimmed();
    if (targetModule.isEmpty()) {
        targetModule = action.module.trimmed();
    }
    if (targetModule == QStringLiteral("shell")) {
        targetModule.clear();
    }
    if (targetModule.isEmpty()) {
        targetModule = m_activeModuleState->getCurrentModule();
    }

    if (targetModule.isEmpty()) {
        LogicNotification notification = createShellError(
            QStringLiteral("LOGIC_ACTION_TARGET_EMPTY"),
            QStringLiteral("No active module is available for action '%1'")
                .arg(describeAction(action)),
            true,
            QStringLiteral("Set payload.targetModule explicitly or switch to an active module first."),
            {{QStringLiteral("actionType"), UiAction::toString(action.actionType)},
             {QStringLiteral("command"), action.payload.value(QStringLiteral("command"))}});
        notification.setSourceActionId(action.actionId);
        notification.setLevel(LogicNotification::Error);
        emit logicNotification(notification);
        return;
    }

    ModuleLogicHandler* handler = m_moduleLogicRegistry->getHandler(targetModule);
    if (handler) {
        handler->handleAction(action);
        return;
    }

    LogicNotification notification = createShellError(
        QStringLiteral("LOGIC_UNROUTED_ACTION"),
        QStringLiteral("No module handler registered for action target '%1'").arg(targetModule),
        true,
        QStringLiteral("Check module registration and action routing."),
        {{QStringLiteral("targetModule"), targetModule},
         {QStringLiteral("actionType"), UiAction::toString(action.actionType)},
         {QStringLiteral("command"), action.payload.value(QStringLiteral("command"))}});
    notification.setSourceActionId(action.actionId);
    notification.setLevel(LogicNotification::Error);
    emit logicNotification(notification);
}

bool LogicRuntime::acceptIncomingSequence(const QString& streamKey,
                                          const QVariantMap& payload,
                                          const QString& actionDescription)
{
    if (!payload.contains(QStringLiteral("seq"))) {
        return true;
    }

    const qint64 seq = payload.value(QStringLiteral("seq")).toLongLong();
    const qint64 lastSeq = m_lastInboundSeqByStream.value(streamKey, std::numeric_limits<qint64>::min());
    if (seq <= lastSeq) {
        emit logicNotification(createShellError(
            QStringLiteral("COMM_STALE_SEQUENCE"),
            QStringLiteral("Ignored stale %1 with seq=%2 on stream '%3'").arg(actionDescription).arg(seq).arg(streamKey),
            true,
            QStringLiteral("Check upstream control-plane ordering and retry behavior."),
            {{QStringLiteral("streamKey"), streamKey},
             {QStringLiteral("seq"), seq},
             {QStringLiteral("lastSeq"), lastSeq}}));
        return false;
    }

    m_lastInboundSeqByStream.insert(streamKey, seq);
    return true;
}
