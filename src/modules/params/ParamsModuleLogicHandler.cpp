#include "ParamsModuleLogicHandler.h"

ParamsModuleLogicHandler::ParamsModuleLogicHandler(QObject* parent)
    : ModuleLogicHandler(QStringLiteral("params"), parent)
{
}

void ParamsModuleLogicHandler::handleAction(const UiAction& action)
{
    if (action.actionType != UiAction::UpdateParameter)
        return;

    const QString key = action.payload.value(QStringLiteral("key")).toString();
    const QVariant value = action.payload.value(QStringLiteral("value"));

    if (key.isEmpty())
        return;

    m_parameters.insert(key, value);

    const bool ready = !m_parameters.isEmpty();
    QVariantMap notifPayload;
    notifPayload.insert(QStringLiteral("parametersValid"), ready);
    notifPayload.insert(QStringLiteral("updatedKey"), key);

    auto notification = LogicNotification::create(
        LogicNotification::ButtonStateChanged,
        LogicNotification::CurrentModule,
        notifPayload);
    notification.setSourceActionId(action.actionId);
    emit logicNotification(notification);
}

void ParamsModuleLogicHandler::onModuleActivated()
{
    QVariantMap payload;
    payload.insert(QStringLiteral("parametersValid"), !m_parameters.isEmpty());
    payload.insert(QStringLiteral("parameterCount"), m_parameters.size());

    emit logicNotification(LogicNotification::create(
        LogicNotification::ButtonStateChanged,
        LogicNotification::CurrentModule,
        payload));
}

void ParamsModuleLogicHandler::onModuleDeactivated()
{
}

void ParamsModuleLogicHandler::onResync()
{
    onModuleActivated();
}

QVariantMap ParamsModuleLogicHandler::getParameters() const
{
    return m_parameters;
}
