#include "AppMessageCenter.h"

#include "logic/registry/ModuleLogicHandler.h"
#include "logic/registry/ModuleLogicRegistry.h"
#include "logic/workflow/ActiveModuleState.h"

namespace {

void registerAppMessageMetaType()
{
    static const int typeId = qRegisterMetaType<AppMessage>("AppMessage");
    Q_UNUSED(typeId);
}

LogicNotification createMessageError(const QString& errorCode,
                                     const QString& message,
                                     const QVariantMap& context)
{
    QVariantMap payload = context;
    payload.insert(QStringLiteral("errorCode"), errorCode);
    payload.insert(QStringLiteral("message"), message);
    payload.insert(QStringLiteral("recoverable"), true);
    payload.insert(QStringLiteral("suggestedAction"),
                   QStringLiteral("Check module registration and message payload."));

    LogicNotification notification = LogicNotification::create(
        LogicNotification::ErrorOccurred,
        LogicNotification::Shell,
        payload);
    notification.setLevel(LogicNotification::Warning);
    return notification;
}

bool isGlobalModuleName(const QString& module)
{
    return module.compare(QStringLiteral("global"), Qt::CaseInsensitive) == 0;
}

} // namespace

AppMessageCenter::AppMessageCenter(QObject* parent)
    : QObject(parent)
{
    registerAppMessageMetaType();
}

void AppMessageCenter::setModuleRegistry(ModuleLogicRegistry* registry)
{
    m_registry = registry;
}

void AppMessageCenter::setActiveModuleState(ActiveModuleState* activeModuleState)
{
    m_activeModuleState = activeModuleState;
}

bool AppMessageCenter::publish(const AppMessage& message)
{
    switch (message.kind) {
    case AppMessageKind::UiIntent:
    case AppMessageKind::ModuleEvent: {
        UiAction action = UiAction::create(
            UiAction::CustomAction,
            message.target.name,
            message.payload);
        action.actionId = message.correlationId.isEmpty() ? message.messageId : message.correlationId;
        action.timestampMs = message.timestampMs;
        if (!message.eventName.isEmpty() && !action.payload.contains(QStringLiteral("eventName"))) {
            action.payload.insert(QStringLiteral("eventName"), message.eventName);
        }
        return routeUiAction(action, message);
    }
    case AppMessageKind::ExternalData: {
        const StateSample sample = StateSample::create(
            message.source.isEmpty() ? QStringLiteral("message_center") : message.source,
            message.target.name,
            message.eventName.isEmpty() ? QStringLiteral("external_data") : message.eventName,
            message.payload);
        return routeStateSample(sample, message);
    }
    case AppMessageKind::ShellEvent:
    case AppMessageKind::GlobalUiEvent:
        return routeShellMessage(message);
    case AppMessageKind::OutboundCommand:
        emit messageAccepted(message);
        return true;
    case AppMessageKind::Error:
        return reject(message,
                      message.error.code.isEmpty() ? QStringLiteral("APP_MESSAGE_ERROR") : message.error.code,
                      message.error.message,
                      message.error.context);
    }

    return reject(message,
                  QStringLiteral("APP_MESSAGE_UNKNOWN_KIND"),
                  QStringLiteral("Unsupported app message kind"));
}

bool AppMessageCenter::dispatchUiIntent(const UiAction& action)
{
    AppMessage message = AppMessage::create(
        AppMessageKind::UiIntent,
        TargetAddress::module(action.module),
        action.payload,
        action.payload.value(QStringLiteral("command")).toString(),
        QStringLiteral("ui"));
    message.correlationId = action.actionId;
    message.timestampMs = action.timestampMs;
    return routeUiAction(action, message);
}

bool AppMessageCenter::dispatchStateSample(const StateSample& sample)
{
    AppMessage message = AppMessage::create(
        AppMessageKind::ExternalData,
        TargetAddress::module(sample.module),
        sample.data,
        sample.sampleType,
        sample.sourceId);
    message.correlationId = sample.sampleId;
    message.timestampMs = sample.timestampMs;
    return routeStateSample(sample, message);
}

bool AppMessageCenter::dispatchModuleEvent(const QString& targetModule,
                                           const QString& eventName,
                                           const QVariantMap& payload,
                                           const QString& sourceModule,
                                           const QString& correlationId)
{
    AppMessage message = AppMessage::create(
        AppMessageKind::ModuleEvent,
        TargetAddress::module(targetModule),
        payload,
        eventName,
        sourceModule);
    if (!correlationId.isEmpty()) {
        message.correlationId = correlationId;
    }
    return publish(message);
}

bool AppMessageCenter::routeUiAction(const UiAction& action, const AppMessage& sourceMessage)
{
    if (!m_registry) {
        return reject(sourceMessage,
                      QStringLiteral("MESSAGE_CENTER_REGISTRY_MISSING"),
                      QStringLiteral("Module runtime registry is not configured"));
    }

    QString targetModule = action.payload.value(QStringLiteral("targetModule")).toString().trimmed();
    if (targetModule.isEmpty()) {
        targetModule = action.module.trimmed();
    }
    if (targetModule == QStringLiteral("shell")) {
        return routeShellMessage(sourceMessage);
    }
    if (targetModule.isEmpty() && m_activeModuleState) {
        targetModule = m_activeModuleState->getCurrentModule();
    }

    targetModule = resolveTargetModule(targetModule);
    if (targetModule.isEmpty()) {
        return reject(sourceMessage,
                      QStringLiteral("APP_MESSAGE_TARGET_EMPTY"),
                      QStringLiteral("No module target is available for app message"),
                      {{QStringLiteral("kind"), AppMessage::kindToString(sourceMessage.kind)}});
    }

    ModuleLogicHandler* handler = m_registry->getHandler(targetModule);
    if (!handler) {
        return reject(sourceMessage,
                      QStringLiteral("APP_MESSAGE_TARGET_UNREGISTERED"),
                      QStringLiteral("No module handler registered for app message target '%1'")
                          .arg(targetModule),
                      {{QStringLiteral("targetModule"), targetModule}});
    }

    handler->handleAction(action);
    emit messageAccepted(sourceMessage);
    return true;
}

bool AppMessageCenter::routeStateSample(const StateSample& sample, const AppMessage& sourceMessage)
{
    if (!m_registry) {
        return reject(sourceMessage,
                      QStringLiteral("MESSAGE_CENTER_REGISTRY_MISSING"),
                      QStringLiteral("Module runtime registry is not configured"));
    }

    if (isGlobalModuleName(sample.module)) {
        const QStringList modules = m_registry->getRegisteredModules();
        for (const QString& moduleId : modules) {
            if (ModuleLogicHandler* handler = m_registry->getHandler(moduleId)) {
                handler->handleStateSample(sample);
            }
        }
        emit messageAccepted(sourceMessage);
        return true;
    }

    QString targetModule = sample.module;
    if (targetModule.isEmpty() && m_activeModuleState) {
        targetModule = m_activeModuleState->getCurrentModule();
    }
    targetModule = resolveTargetModule(targetModule);

    ModuleLogicHandler* handler = m_registry->getHandler(targetModule);
    if (!handler) {
        return reject(sourceMessage,
                      QStringLiteral("APP_MESSAGE_DATA_TARGET_UNREGISTERED"),
                      QStringLiteral("No module handler registered for state sample target '%1'")
                          .arg(targetModule),
                      {{QStringLiteral("sampleId"), sample.sampleId},
                       {QStringLiteral("sampleType"), sample.sampleType}});
    }

    StateSample routed = sample;
    routed.module = targetModule;
    handler->handleStateSample(routed);
    emit messageAccepted(sourceMessage);
    return true;
}

bool AppMessageCenter::routeShellMessage(const AppMessage& message)
{
    LogicNotification notification = LogicNotification::create(
        LogicNotification::CustomEvent,
        message.target.isGlobalUi() ? LogicNotification::AllModules : LogicNotification::Shell,
        message.payload);
    notification.payload.insert(QStringLiteral("eventName"), message.eventName);
    notification.payload.insert(QStringLiteral("messageKind"), AppMessage::kindToString(message.kind));
    notification.setSourceActionId(message.correlationId);
    emit logicNotification(notification);
    emit messageAccepted(message);
    return true;
}

bool AppMessageCenter::reject(const AppMessage& message,
                              const QString& errorCode,
                              const QString& text,
                              const QVariantMap& context)
{
    QVariantMap errorContext = context;
    errorContext.insert(QStringLiteral("messageId"), message.messageId);
    errorContext.insert(QStringLiteral("target"), message.target.name);
    errorContext.insert(QStringLiteral("kind"), AppMessage::kindToString(message.kind));

    LogicNotification notification = createMessageError(errorCode, text, errorContext);
    notification.setSourceActionId(message.correlationId);
    emit messageRejected(message, notification);
    emit logicNotification(notification);
    return false;
}

QString AppMessageCenter::resolveTargetModule(const QString& targetName) const
{
    if (!m_registry) {
        return QString();
    }

    return m_registry->resolveModuleId(targetName.trimmed());
}
