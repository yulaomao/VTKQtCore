#include "UiActionDispatcher.h"

#include "logic/gateway/ILogicGateway.h"

namespace {

QVariantMap withCommandPayload(const QString& command,
                               const QVariantMap& payload)
{
    QVariantMap result = payload;
    result.insert(QStringLiteral("command"), command);
    return result;
}

}

UiActionDispatcher::UiActionDispatcher(const QString& sourceModule,
                                       ILogicGateway* gateway,
                                       QObject* parent)
    : QObject(parent)
    , m_sourceModule(sourceModule)
    , m_gateway(gateway)
{
}

QString UiActionDispatcher::getSourceModule() const
{
    return m_sourceModule;
}

void UiActionDispatcher::sendAction(UiAction::ActionType type,
                                    const QVariantMap& payload)
{
    sendAction(createAction(type, payload));
}

void UiActionDispatcher::sendAction(const UiAction& action)
{
    if (m_gateway) {
        m_gateway->sendAction(action);
    }
    emit actionDispatched(action);
}

void UiActionDispatcher::sendCommand(const QString& command,
                                     const QVariantMap& payload)
{
    if (command.trimmed().isEmpty()) {
        return;
    }

    sendAction(UiAction::CustomAction, withCommandPayload(command, payload));
}

void UiActionDispatcher::sendTargetedCommand(const QString& targetModule,
                                             const QString& command,
                                             const QVariantMap& payload)
{
    if (targetModule.trimmed().isEmpty() || command.trimmed().isEmpty()) {
        return;
    }

    QVariantMap targetedPayload = payload;
    targetedPayload.insert(QStringLiteral("targetModule"), targetModule);
    sendCommand(command, targetedPayload);
}

void UiActionDispatcher::requestModuleSwitch(const QString& targetModule)
{
    if (targetModule.trimmed().isEmpty()) {
        return;
    }

    sendAction(
        UiAction::RequestSwitchModule,
        {{QStringLiteral("targetModule"), targetModule}});
}

void UiActionDispatcher::requestResync(const QString& reason) const
{
    if (m_gateway) {
        m_gateway->requestResync(reason);
    }
}

UiAction UiActionDispatcher::createAction(UiAction::ActionType type,
                                          const QVariantMap& payload) const
{
    return UiAction::create(type, m_sourceModule, payload);
}