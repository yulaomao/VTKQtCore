#include "LogicRuntime.h"

#include "scene/SceneGraph.h"
#include "workflow/WorkflowStateMachine.h"
#include "registry/ModuleLogicRegistry.h"
#include "registry/ModuleLogicHandler.h"

#include <limits>
#include <QSet>

namespace {

QString normalizeKey(const QString& value)
{
    QString normalized = value.trimmed().toLower();
    normalized.replace(QLatin1Char('-'), QLatin1Char('_'));
    return normalized;
}

bool parseActionType(const QString& rawType, UiAction::ActionType& actionType)
{
    return UiAction::fromString(rawType, actionType);
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

LogicNotification createWorkflowRejectedNotification(const WorkflowDecision& decision,
                                                    UiAction::ActionType actionType,
                                                    const QString& actionId)
{
    LogicNotification notification = createShellError(
        QStringLiteral("WORKFLOW_ACTION_REJECTED"),
        decision.message,
        true,
        QStringLiteral("Wait for workflow update or choose an enterable module."),
        {{QStringLiteral("reasonCode"), decision.reasonCode},
         {QStringLiteral("targetModule"), decision.targetModule},
         {QStringLiteral("currentModule"), decision.currentModule},
            {QStringLiteral("actionType"), UiAction::toString(actionType)}});
    notification.setSourceActionId(actionId);
    notification.setLevel(LogicNotification::Warning);
    return notification;
}

} // namespace

LogicRuntime::LogicRuntime(QObject* parent)
    : QObject(parent)
    , m_sceneGraph(new SceneGraph(this))
    , m_workflowStateMachine(new WorkflowStateMachine(this))
    , m_moduleLogicRegistry(new ModuleLogicRegistry(this))
{
}

SceneGraph* LogicRuntime::getSceneGraph() const
{
    return m_sceneGraph;
}

WorkflowStateMachine* LogicRuntime::getWorkflowStateMachine() const
{
    return m_workflowStateMachine;
}

ModuleLogicRegistry* LogicRuntime::getModuleLogicRegistry() const
{
    return m_moduleLogicRegistry;
}

void LogicRuntime::registerModuleHandler(ModuleLogicHandler* handler)
{
    if (!handler) {
        return;
    }

    handler->setSceneGraph(m_sceneGraph);
    m_moduleLogicRegistry->registerHandler(handler);

    connect(handler, &ModuleLogicHandler::logicNotification,
            this, &LogicRuntime::logicNotification);
}

void LogicRuntime::onActionReceived(const UiAction& action)
{
    const WorkflowDecision actionDecision = m_workflowStateMachine->evaluateAction(action);
    if (!actionDecision.allowed) {
        emit logicNotification(createWorkflowRejectedNotification(
            actionDecision, action.actionType, action.actionId));
        return;
    }

    switch (action.actionType) {
    case UiAction::NextStep: {
        switchToModule(actionDecision.targetModule, action.actionId);
        break;
    }
    case UiAction::PrevStep: {
        switchToModule(actionDecision.targetModule, action.actionId);
        break;
    }
    case UiAction::RequestSwitchModule: {
        switchToModule(actionDecision.targetModule, action.actionId);
        break;
    }
    default:
        routeToModuleHandler(action);
        break;
    }
}

void LogicRuntime::onControlMessageReceived(const QString& module, const QVariantMap& payload)
{
    const QString streamKey = QStringLiteral("control:%1").arg(
        module.isEmpty() ? QStringLiteral("shell") : module);
    if (!acceptIncomingSequence(streamKey, payload, QStringLiteral("control message"))) {
        return;
    }

    UiAction::ActionType actionType = UiAction::CustomAction;
    const QString rawActionType = payload.value(QStringLiteral("actionType")).toString();
    if (!parseActionType(rawActionType, actionType)) {
        emit logicNotification(createShellError(
            QStringLiteral("COMM_UNSUPPORTED_ACTION"),
            QStringLiteral("Unsupported control action type '%1'").arg(rawActionType),
            true,
            QStringLiteral("Verify server control message mapping."),
            {{QStringLiteral("module"), module}}));
        return;
    }

    QString targetModule = module;
    if (targetModule.isEmpty()) {
        targetModule = payload.value(QStringLiteral("module")).toString();
    }
    if (targetModule.isEmpty()) {
        targetModule = actionType == UiAction::RequestSwitchModule
            ? QStringLiteral("shell")
            : m_workflowStateMachine->getCurrentModule();
    }

    UiAction action = UiAction::create(actionType, targetModule, extractPayloadMap(payload));
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
            UiAction::RequestSwitchModule,
            QStringLiteral("shell"),
            {{QStringLiteral("targetModule"), targetModule}});
        const QString sourceActionId = payload.value(QStringLiteral("msgId")).toString();
        if (!sourceActionId.isEmpty()) {
            action.actionId = sourceActionId;
        }
        onActionReceived(action);
        return;
    }

    if (normalizedCommand == QStringLiteral("next_step") ||
        normalizedCommand == QStringLiteral("prev_step")) {
        UiAction action = UiAction::create(
            normalizedCommand == QStringLiteral("next_step")
                ? UiAction::NextStep
                : UiAction::PrevStep,
            QStringLiteral("shell"));
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

    if (normalizedCommand == QStringLiteral("set_enterable_modules") ||
        normalizedCommand == QStringLiteral("workflow_update")) {
        const QStringList moduleIds = toStringList(commandPayload.value(QStringLiteral("enterableModules")));
        if (!moduleIds.isEmpty()) {
            QSet<QString> enterableModules;
            for (const QString& moduleId : moduleIds) {
                enterableModules.insert(moduleId);
            }
            m_workflowStateMachine->setEnterableModules(enterableModules);
        }

        const QString currentModule = commandPayload.value(QStringLiteral("currentModule")).toString();
        const QString sourceActionId = payload.value(QStringLiteral("msgId")).toString();
        if (!currentModule.isEmpty()) {
            const WorkflowDecision decision = m_workflowStateMachine->evaluateSwitchTo(currentModule);
            if (decision.allowed && currentModule != m_workflowStateMachine->getCurrentModule()) {
                switchToModule(currentModule, sourceActionId);
            } else if (!decision.allowed) {
                LogicNotification notification = createShellError(
                    QStringLiteral("WORKFLOW_UPDATE_REJECTED"),
                    decision.message,
                    true,
                    QStringLiteral("Verify the server workflow update payload."),
                    {{QStringLiteral("reasonCode"), decision.reasonCode},
                     {QStringLiteral("targetModule"), decision.targetModule},
                     {QStringLiteral("currentModule"), decision.currentModule}});
                notification.setSourceActionId(sourceActionId);
                notification.setLevel(LogicNotification::Warning);
                emit logicNotification(notification);
            } else {
                LogicNotification notification = LogicNotification::create(
                    LogicNotification::WorkflowChanged,
                    LogicNotification::Shell,
                    m_workflowStateMachine->createSnapshot());
                notification.setSourceActionId(sourceActionId);
                emit logicNotification(notification);
            }
        } else {
            LogicNotification notification = LogicNotification::create(
                LogicNotification::WorkflowChanged,
                LogicNotification::Shell,
                m_workflowStateMachine->createSnapshot());
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
    QString targetModule = sample.module;
    if (targetModule.isEmpty()) {
        targetModule = m_workflowStateMachine->getCurrentModule();
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
        LogicNotification::WorkflowChanged,
        LogicNotification::Shell,
        m_workflowStateMachine->createSnapshot());
    notification.payload.insert(QStringLiteral("resyncRequested"), true);
    notification.payload.insert(QStringLiteral("reason"), reason);
    emit logicNotification(notification);
}

void LogicRuntime::switchToModule(const QString& targetModule, const QString& sourceActionId)
{
    const QString oldModule = m_workflowStateMachine->getCurrentModule();

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

    // Update workflow state
    m_workflowStateMachine->setCurrentModule(targetModule);

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

    LogicNotification workflowNotification = LogicNotification::create(
        LogicNotification::WorkflowChanged,
        LogicNotification::Shell,
        m_workflowStateMachine->createSnapshot());
    workflowNotification.setSourceActionId(sourceActionId);
    emit logicNotification(workflowNotification);
}

void LogicRuntime::routeToModuleHandler(const UiAction& action)
{
    const QString targetModule = action.module.isEmpty()
        ? m_workflowStateMachine->getCurrentModule()
        : action.module;
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
         {QStringLiteral("actionType"), UiAction::toString(action.actionType)}});
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
