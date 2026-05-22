#include "ModuleLogicHandler.h"

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

bool ModuleLogicHandler::playPromptAudioPreset(const QString& presetId)
{
    return m_moduleInvoker && m_moduleInvoker->playPromptAudioPreset(presetId);
}

bool ModuleLogicHandler::playPromptAudioSource(const QString& source)
{
    return m_moduleInvoker && m_moduleInvoker->playPromptAudioSource(source);
}

bool ModuleLogicHandler::registerPromptAudioPreset(const QString& presetId, const QString& source)
{
    return m_moduleInvoker && m_moduleInvoker->registerPromptAudioPreset(presetId, source);
}

void ModuleLogicHandler::stopPromptAudio()
{
    if (m_moduleInvoker) {
        m_moduleInvoker->stopPromptAudio();
    }
}

// ---------------------------------------------------------------------------
// Data dispatch — default subscription shim
// ---------------------------------------------------------------------------
// Polling data now arrives as one aggregated StateSample per module directly
// from LogicRuntime. Subscription data still uses the compatibility wrapper
// below so existing module implementations can continue to consume
// handleStateSample().

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
