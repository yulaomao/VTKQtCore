#include "ParamsModuleLogicHandler.h"

#include "ParamsUiCommands.h"

ParamsModuleLogicHandler::ParamsModuleLogicHandler(QObject* parent)
    : ModuleLogicHandler(QStringLiteral("params"), parent)
{
}

void ParamsModuleLogicHandler::handleAction(const UiAction& action)
{
    if (action.actionType != UiAction::CustomAction) {
        return;
    }

    const QString command = action.payload.value(QStringLiteral("command")).toString().trimmed();
    if (command == ParamsUiCommands::applyParameters()) {
        const QVariantMap parameters = action.payload.value(QStringLiteral("parameters")).toMap();
        if (parameters.isEmpty()) {
            return;
        }

        for (auto it = parameters.cbegin(); it != parameters.cend(); ++it) {
            m_parameters.insert(it.key(), it.value());
        }

        const bool ready = !m_parameters.isEmpty();
        const QStringList updatedKeys = parameters.keys();
        QVariantMap notificationPayload;
        notificationPayload.insert(QStringLiteral("parametersValid"), ready);
        notificationPayload.insert(QStringLiteral("updatedKeys"), QVariant::fromValue(updatedKeys));
        notificationPayload.insert(QStringLiteral("parameterCount"), m_parameters.size());

        LogicNotification notification = LogicNotification::create(
            LogicNotification::ButtonStateChanged,
            LogicNotification::CurrentModule,
            notificationPayload);
        notification.setSourceActionId(action.actionId);
        emit logicNotification(notification);
        return;
    }

    if (command != ParamsUiCommands::updateParameter()) {
        return;
    }

    const QString key = action.payload.value(QStringLiteral("key")).toString();
    const QVariant value = action.payload.value(QStringLiteral("value"));

    if (key.isEmpty())
        return;

    m_parameters.insert(key, value);

    const bool ready = !m_parameters.isEmpty();
    QVariantMap notifPayload;
    notifPayload.insert(QStringLiteral("parametersValid"), ready);
    notifPayload.insert(QStringLiteral("updatedKey"), key);
    notifPayload.insert(QStringLiteral("parameterCount"), m_parameters.size());

    auto notification = LogicNotification::create(
        LogicNotification::ButtonStateChanged,
        LogicNotification::CurrentModule,
        notifPayload);
    notification.setSourceActionId(action.actionId);
    emit logicNotification(notification);
}

void ParamsModuleLogicHandler::handleStateSample(const StateSample& sample)
{
    QVariantMap incomingParameters;

    const QVariantMap values = sample.data.value(QStringLiteral("values")).toMap();
    if (!values.isEmpty()) {
        for (auto it = values.cbegin(); it != values.cend(); ++it) {
            const QVariantMap payload = it.value().toMap();
            if (payload.isEmpty()) {
                continue;
            }
            for (auto payloadIt = payload.cbegin(); payloadIt != payload.cend(); ++payloadIt) {
                incomingParameters.insert(payloadIt.key(), payloadIt.value());
            }
        }
    } else {
        incomingParameters = sample.data.value(QStringLiteral("parameters")).toMap();
        if (incomingParameters.isEmpty()) {
            incomingParameters = sample.data;
            incomingParameters.remove(QStringLiteral("channel"));
        }
    }

    if (incomingParameters.isEmpty()) {
        return;
    }

    for (auto it = incomingParameters.cbegin(); it != incomingParameters.cend(); ++it) {
        m_parameters.insert(it.key(), it.value());
    }

    emit logicNotification(LogicNotification::create(
        LogicNotification::ButtonStateChanged,
        LogicNotification::CurrentModule,
        {{QStringLiteral("parametersValid"), !m_parameters.isEmpty()},
         {QStringLiteral("parameterCount"), m_parameters.size()},
         {QStringLiteral("sourceSampleId"), sample.sampleId}}));
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
