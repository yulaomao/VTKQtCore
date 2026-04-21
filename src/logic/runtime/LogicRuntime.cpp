#include "LogicRuntime.h"

#include "communication/hub/IRedisCommandAccess.h"
#include "logic/runtime/GlobalPollingSampleParser.h"
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

QVariantMap extractPayloadMap(const QVariantMap& input)
{
    QVariantMap payload = input.value(QStringLiteral("payload")).toMap();
    if (payload.isEmpty()) {
        payload = input;
    }

    payload.remove(QStringLiteral("category"));
    payload.remove(QStringLiteral("msgId"));
    payload.remove(QStringLiteral("module"));
    payload.remove(QStringLiteral("actionId"));
    payload.remove(QStringLiteral("actionType"));
    payload.remove(QStringLiteral("commandType"));
    payload.remove(QStringLiteral("timestampMs"));
    return payload;
}

QStringList toStringList(const QVariant& value)
{
    if (value.canConvert<QStringList>()) {
        return value.toStringList();
    }

    QStringList result;
    const QVariantList list = value.toList();
    for (const QVariant& item : list) {
        const QString text = item.toString();
        if (!text.isEmpty()) {
            result.append(text);
        }
    }
    return result;
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

bool isGlobalPollingBatchSample(const StateSample& sample)
{
    return sample.module.isEmpty() && sample.sampleType == QStringLiteral("global_poll_batch");
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

void LogicRuntime::setGlobalPollingSampleParser(GlobalPollingSampleParser* parser)
{
    if (m_globalPollingSampleParser == parser) {
        return;
    }

    if (m_globalPollingSampleParser) {
        m_globalPollingSampleParser->deleteLater();
    }

    m_globalPollingSampleParser = parser;
    if (m_globalPollingSampleParser && !m_globalPollingSampleParser->parent()) {
        m_globalPollingSampleParser->setParent(this);
    }
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

void LogicRuntime::onControlMessageReceived(const QString& module, const QVariantMap& payload)
{
    const QString streamKey = QStringLiteral("control:%1").arg(
        module.isEmpty() ? QStringLiteral("shell") : module);
    if (!acceptIncomingSequence(streamKey, payload, QStringLiteral("control message"))) {
        return;
    }

    QVariantMap actionPayload = extractPayloadMap(payload);
    const QString rawActionType = normalizeKey(payload.value(QStringLiteral("actionType")).toString());
    if (!rawActionType.isEmpty() &&
        rawActionType != UiAction::toString(UiAction::CustomAction) &&
        actionCommand(actionPayload).isEmpty()) {
        actionPayload.insert(QStringLiteral("command"), rawActionType);
    }

    QString targetModule = module;
    if (targetModule.isEmpty()) {
        targetModule = payload.value(QStringLiteral("module")).toString();
    }
    if (targetModule.isEmpty()) {
        targetModule = payload.value(QStringLiteral("targetModule")).toString();
    }
    if (targetModule.isEmpty()) {
        targetModule = isSwitchModuleCommand(actionCommand(actionPayload))
            ? QStringLiteral("shell")
            : m_activeModuleState->getCurrentModule();
    }

    UiAction action = UiAction::create(UiAction::CustomAction, targetModule, actionPayload);
    const QString externalActionId = payload.value(QStringLiteral("actionId")).toString().isEmpty()
        ? payload.value(QStringLiteral("msgId")).toString()
        : payload.value(QStringLiteral("actionId")).toString();
    if (!externalActionId.isEmpty()) {
        action.actionId = externalActionId;
    }
    if (payload.contains(QStringLiteral("timestampMs"))) {
        action.timestampMs = payload.value(QStringLiteral("timestampMs")).toLongLong();
    }

    onActionReceived(action);
}

void LogicRuntime::onServerCommandReceived(const QString& commandType, const QVariantMap& payload)
{
    const QString streamKey = QStringLiteral("command:%1").arg(normalizeKey(commandType));
    if (!acceptIncomingSequence(streamKey, payload, QStringLiteral("server command"))) {
        return;
    }

    const QString normalizedCommand = normalizeKey(commandType);
    const QVariantMap commandPayload = extractPayloadMap(payload);

    if (normalizedCommand == QStringLiteral("switch_module") ||
        normalizedCommand == QStringLiteral("request_switch_module")) {
        const QString targetModule = commandPayload.value(QStringLiteral("targetModule")).toString();
        if (targetModule.isEmpty()) {
            emit logicNotification(createShellError(
                QStringLiteral("COMM_INVALID_COMMAND"),
                QStringLiteral("switch_module command missing targetModule"),
                true,
                QStringLiteral("Resend the server command with a target module.")));
            return;
        }

        UiAction action = UiAction::create(
            UiAction::CustomAction,
            QStringLiteral("shell"),
            {{QStringLiteral("command"), normalizedCommand},
             {QStringLiteral("targetModule"), targetModule}});
        const QString sourceActionId = payload.value(QStringLiteral("msgId")).toString();
        if (!sourceActionId.isEmpty()) {
            action.actionId = sourceActionId;
        }
        onActionReceived(action);
        return;
    }

    if (normalizedCommand == QStringLiteral("next_step") ||
        normalizedCommand == QStringLiteral("prev_step")) {
        QString targetModule = commandPayload.value(QStringLiteral("targetModule")).toString();
        if (targetModule.isEmpty()) {
            targetModule = commandPayload.value(QStringLiteral("module")).toString();
        }
        if (targetModule.isEmpty()) {
            targetModule = m_activeModuleState->getCurrentModule();
        }

        QVariantMap actionPayload = commandPayload;
        actionPayload.insert(QStringLiteral("command"), normalizedCommand);

        UiAction action = UiAction::create(
            UiAction::CustomAction,
            targetModule,
            actionPayload);
        const QString sourceActionId = payload.value(QStringLiteral("msgId")).toString();
        if (!sourceActionId.isEmpty()) {
            action.actionId = sourceActionId;
        }
        onActionReceived(action);
        return;
    }

    if (normalizedCommand == QStringLiteral("resync") ||
        normalizedCommand == QStringLiteral("request_resync") ||
        normalizedCommand == QStringLiteral("resync_request")) {
        requestResync(commandPayload.value(QStringLiteral("reason")).toString());
        return;
    }

    if (normalizedCommand == QStringLiteral("datagen_test_action") ||
        normalizedCommand == QStringLiteral("datagen_custom_action")) {
        ModuleInvokeRequest request = ModuleInvokeRequest::create(
            QStringLiteral("server"),
            QStringLiteral("datagen"),
            commandPayload.value(QStringLiteral("command")).toString(),
            commandPayload);
        const ModuleInvokeResult result = invokeModule(request);
        if (!result.ok) {
            emit logicNotification(createShellError(
                QStringLiteral("SERVER_COMMAND_INVOKE_FAILED"),
                result.message,
                true,
                QStringLiteral("Verify datagen invoke routing and payload."),
                {{QStringLiteral("targetModule"), QStringLiteral("datagen")},
                 {QStringLiteral("method"), request.method},
                 {QStringLiteral("errorCode"), result.errorCode}}));
        }
        return;
    }

    if (normalizedCommand == QStringLiteral("active_module_sync")) {
        const QString currentModule = commandPayload.value(QStringLiteral("currentModule")).toString();
        const QString sourceActionId = payload.value(QStringLiteral("msgId")).toString();
        if (!currentModule.isEmpty()) {
            if (m_moduleLogicRegistry->getHandler(currentModule) &&
                currentModule != m_activeModuleState->getCurrentModule()) {
                switchToModule(currentModule, sourceActionId);
            } else if (!m_moduleLogicRegistry->getHandler(currentModule)) {
                LogicNotification notification = createShellError(
                    QStringLiteral("ACTIVE_MODULE_SYNC_TARGET_UNREGISTERED"),
                    QStringLiteral("Active module sync references unregistered module '%1'").arg(currentModule),
                    true,
                    QStringLiteral("Verify the server active_module_sync payload and module registration."),
                    {{QStringLiteral("targetModule"), currentModule}});
                notification.setSourceActionId(sourceActionId);
                notification.setLevel(LogicNotification::Warning);
                emit logicNotification(notification);
            } else {
                LogicNotification notification = LogicNotification::create(
                    LogicNotification::ActiveModuleChanged,
                    LogicNotification::Shell,
                    m_activeModuleState->createSnapshot());
                notification.setSourceActionId(sourceActionId);
                emit logicNotification(notification);
            }
        } else {
            LogicNotification notification = LogicNotification::create(
                LogicNotification::ActiveModuleChanged,
                LogicNotification::Shell,
                m_activeModuleState->createSnapshot());
            notification.setSourceActionId(sourceActionId);
            emit logicNotification(notification);
        }
        return;
    }

    emit logicNotification(createShellError(
        QStringLiteral("COMM_UNSUPPORTED_COMMAND"),
        QStringLiteral("Unsupported server command '%1'").arg(commandType),
        true,
        QStringLiteral("Verify server command routing.")));
}

void LogicRuntime::onStateSampleReceived(const StateSample& sample)
{
    if (isGlobalPollingBatchSample(sample)) {
        if (!m_globalPollingSampleParser) {
            emit logicNotification(createShellError(
                QStringLiteral("DATA_GLOBAL_POLLING_PARSER_MISSING"),
                QStringLiteral("Global polling batch received without a configured parser"),
                true,
                QStringLiteral("Configure a GlobalPollingSampleParser before starting Redis mode."),
                {{QStringLiteral("sampleId"), sample.sampleId}}));
            return;
        }

        const QVector<StateSample> routedSamples = m_globalPollingSampleParser->parse(sample);
        for (const StateSample& routedSample : routedSamples) {
            onStateSampleReceived(routedSample);
        }
        return;
    }

    QString targetModule = sample.module;
    if (targetModule.isEmpty()) {
        targetModule = m_activeModuleState->getCurrentModule();
    }

    ModuleLogicHandler* handler = m_moduleLogicRegistry->getHandler(targetModule);
    if (!handler) {
        emit logicNotification(createShellError(
            QStringLiteral("DATA_UNROUTED_SAMPLE"),
            QStringLiteral("No module handler registered for state sample target '%1'").arg(targetModule),
            true,
            QStringLiteral("Check sample.module and module registration."),
            {{QStringLiteral("sampleId"), sample.sampleId},
             {QStringLiteral("sampleType"), sample.sampleType}}));
        return;
    }

    handler->handleStateSample(sample);
}

void LogicRuntime::onCommunicationError(const QString& source, const QString& errorMessage)
{
    emit logicNotification(createShellError(
        QStringLiteral("COMM_CHANNEL_ERROR"),
        errorMessage,
        true,
        QStringLiteral("Check Redis connectivity and request resync if needed."),
        {{QStringLiteral("source"), source}}));
}

void LogicRuntime::onCommunicationIssue(const QString& source,
                                        const QString& severity,
                                        const QString& errorCode,
                                        const QString& errorMessage,
                                        const QVariantMap& context)
{
    QVariantMap payload = context;
    payload.insert(QStringLiteral("source"), source);
    payload.insert(QStringLiteral("severity"), severity);

    LogicNotification notification = createShellError(
        errorCode,
        errorMessage,
        severity != QStringLiteral("critical"),
        QStringLiteral("Inspect communication health metrics and upstream transport state."),
        payload);

    if (severity == QStringLiteral("critical") || severity == QStringLiteral("error")) {
        notification.setLevel(LogicNotification::Error);
    } else {
        notification.setLevel(LogicNotification::Warning);
    }

    emit logicNotification(notification);
}

void LogicRuntime::onCommunicationHealthChanged(const QVariantMap& healthSnapshot)
{
    LogicNotification notification = LogicNotification::create(
        LogicNotification::CustomEvent,
        LogicNotification::Shell,
        healthSnapshot);
    notification.payload.insert(QStringLiteral("eventName"), QStringLiteral("communication_health"));
    emit logicNotification(notification);
}

void LogicRuntime::onConnectionStateChanged(const QString& state)
{
    QString shellState = state;
    if (state == QStringLiteral("Reconnecting")) {
        shellState = QStringLiteral("Degraded");
    }

    LogicNotification notification = LogicNotification::create(
        LogicNotification::ConnectionStateChanged,
        LogicNotification::Shell,
        {{QStringLiteral("state"), shellState},
         {QStringLiteral("rawState"), state}});
    emit logicNotification(notification);
}

void LogicRuntime::requestResync(const QString& reason)
{
    const QStringList registeredModules = m_moduleLogicRegistry->getRegisteredModules();
    for (const QString& moduleId : registeredModules) {
        ModuleLogicHandler* handler = m_moduleLogicRegistry->getHandler(moduleId);
        if (handler) {
            handler->onResync();
        }
    }

    LogicNotification notification = LogicNotification::create(
        LogicNotification::ActiveModuleChanged,
        LogicNotification::Shell,
        m_activeModuleState->createSnapshot());
    notification.payload.insert(QStringLiteral("resyncRequested"), true);
    notification.payload.insert(QStringLiteral("reason"), reason);
    emit logicNotification(notification);
}

void LogicRuntime::switchToModule(const QString& targetModule, const QString& sourceActionId)
{
    const QString oldModule = m_activeModuleState->getCurrentModule();

    if (targetModule.isEmpty() || targetModule == oldModule) {
        return;
    }

    // Deactivate old module handler
    if (!oldModule.isEmpty()) {
        ModuleLogicHandler* oldHandler = m_moduleLogicRegistry->getHandler(oldModule);
        if (oldHandler) {
            oldHandler->onModuleDeactivated();
        }
    }

    // Update active module state
    m_activeModuleState->setCurrentModule(targetModule);

    // Activate new module handler
    ModuleLogicHandler* newHandler = m_moduleLogicRegistry->getHandler(targetModule);
    if (newHandler) {
        newHandler->onModuleActivated();
    }

    // Emit ModuleChanged notification with Shell scope
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
