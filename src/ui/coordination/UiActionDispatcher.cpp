#include "UiActionDispatcher.h"

#include "ModuleUiEvent.h"
#include "logic/runtime/ILogicRuntimePort.h"

namespace {

QString switchModuleCommand()
{
    return QStringLiteral("switch_module");
}

QVariantMap withCommandPayload(const QString& command,
                               const QVariantMap& payload)
{
    QVariantMap result = payload;
    result.insert(QStringLiteral("command"), command);
    return result;
}

}

UiActionDispatcher::UiActionDispatcher(const QString& sourceModule,
                                       ILogicRuntimePort* runtimePort,
                                       QObject* parent)
    : QObject(parent)
    , m_sourceModule(sourceModule)
    , m_runtimePort(runtimePort)
{
}

QString UiActionDispatcher::getSourceModule() const
{
    return m_sourceModule;
}

void UiActionDispatcher::sendAction(const UiAction& action)
{
    if (m_runtimePort) {
        m_runtimePort->sendAction(action);
    }

    emit actionDispatched(action);
}

void UiActionDispatcher::sendCommand(const QString& command,
                                     const QVariantMap& payload)
{
    if (command.trimmed().isEmpty()) {
        return;
    }

    sendAction(UiAction::create(
        UiAction::CustomAction,
        m_sourceModule,
        withCommandPayload(command, payload)));
}

void UiActionDispatcher::sendTargetedCommand(const QString& targetModule,
                                             const QString& command,
                                             const QVariantMap& payload)
{
    sendToTarget(targetModule, command, payload);
}

void UiActionDispatcher::sendToTarget(const QString& targetName,
                                      const QString& command,
                                      const QVariantMap& payload)
{
    if (targetName.trimmed().isEmpty() || command.trimmed().isEmpty()) {
        return;
    }

    QVariantMap targetedPayload = payload;
    // targetName is the generic registered-address field; targetModule keeps
    // compatibility with LogicRuntime's existing module-routing contract.
    targetedPayload.insert(QStringLiteral("targetName"), targetName.trimmed());
    targetedPayload.insert(QStringLiteral("targetModule"), targetName.trimmed());
    sendCommand(command, targetedPayload);
}

void UiActionDispatcher::sendModuleUiEvent(const QString& targetModule,
                                           const QString& eventName,
                                           const QVariantMap& payload)
{
    if (targetModule.trimmed().isEmpty() || eventName.trimmed().isEmpty()) {
        return;
    }

    QVariantMap eventPayload = ModuleUiEvent::createActionPayload(eventName, payload);
    eventPayload.insert(QStringLiteral("targetModule"), targetModule);
    sendAction(UiAction::create(UiAction::CustomAction, m_sourceModule, eventPayload));
}

void UiActionDispatcher::requestModuleSwitch(const QString& targetModule)
{
    if (targetModule.trimmed().isEmpty()) {
        return;
    }

    sendCommand(
        switchModuleCommand(),
        {{QStringLiteral("targetModule"), targetModule}});
}

void UiActionDispatcher::requestResync(const QString& reason)
{
    if (m_runtimePort) {
        m_runtimePort->requestResync(reason);
    }

    emit resyncRequested(reason);
}
