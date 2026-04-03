#include "UiActionDispatcher.h"

#include "logic/gateway/ILogicGateway.h"

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

bool UiActionDispatcher::sendAction(const UiAction& action)
{
    const bool accepted = m_gateway && m_gateway->sendAction(action);
    emit actionDispatched(action, accepted);
    return accepted;
}

bool UiActionDispatcher::sendCommand(const QString& command,
                                     const QVariantMap& payload)
{
    if (command.trimmed().isEmpty()) {
        return false;
    }

    return sendAction(UiAction::create(
        UiAction::CustomAction,
        m_sourceModule,
        withCommandPayload(command, payload)));
}

bool UiActionDispatcher::sendTargetedCommand(const QString& targetModule,
                                             const QString& command,
                                             const QVariantMap& payload)
{
    if (targetModule.trimmed().isEmpty() || command.trimmed().isEmpty()) {
        return false;
    }

    QVariantMap targetedPayload = payload;
    targetedPayload.insert(QStringLiteral("targetModule"), targetModule);
    return sendCommand(command, targetedPayload);
}

bool UiActionDispatcher::requestModuleSwitch(const QString& targetModule)
{
    if (targetModule.trimmed().isEmpty()) {
        return false;
    }

    return sendCommand(
        switchModuleCommand(),
        {{QStringLiteral("targetModule"), targetModule}});
}

void UiActionDispatcher::requestResync(const QString& reason) const
{
    if (m_gateway) {
        m_gateway->requestResync(reason);
    }
}
