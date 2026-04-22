#include "ParamsModuleLogicHandler.h"

#include "ParamsUiCommands.h"

#include <QByteArray>
#include <QJsonDocument>

namespace {

QString paramsStateRedisKey()
{
    return QStringLiteral("state.params.latest");
}

QVariantMap decodeJsonValue(const QVariant& value)
{
    const QByteArray bytes = value.toByteArray();
    if (!bytes.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(bytes);
        if (doc.isObject()) {
            return doc.object().toVariantMap();
        }
    }
    return value.toMap();
}

}

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

        writeRedisJsonValue(paramsStateRedisKey(), m_parameters);

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
    writeRedisJsonValue(paramsStateRedisKey(), m_parameters);

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

void ParamsModuleLogicHandler::handlePollData(const QString& key, const QVariant& value)
{
    Q_UNUSED(key)

    const QVariantMap incomingParameters = decodeJsonValue(value);
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
         {QStringLiteral("parameterCount"), m_parameters.size()}}));
}

void ParamsModuleLogicHandler::handleSubscribeData(const QString& channel,
                                                   const QVariantMap& data)
{
    Q_UNUSED(channel)

    if (data.isEmpty()) {
        return;
    }

    for (auto it = data.cbegin(); it != data.cend(); ++it) {
        m_parameters.insert(it.key(), it.value());
    }

    emit logicNotification(LogicNotification::create(
        LogicNotification::ButtonStateChanged,
        LogicNotification::CurrentModule,
        {{QStringLiteral("parametersValid"), !m_parameters.isEmpty()},
         {QStringLiteral("parameterCount"), m_parameters.size()}}));
}

void ParamsModuleLogicHandler::onModuleActivated()
{
    if (m_parameters.isEmpty() && hasRedisCommandAccess()) {
        const QVariantMap redisSnapshot = readRedisJsonValue(paramsStateRedisKey());
        if (!redisSnapshot.isEmpty()) {
            m_parameters = redisSnapshot;
        }
    }

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
    if (hasRedisCommandAccess()) {
        const QVariantMap redisSnapshot = readRedisJsonValue(paramsStateRedisKey());
        if (!redisSnapshot.isEmpty()) {
            m_parameters = redisSnapshot;
        }
    }

    onModuleActivated();
}

QVariantMap ParamsModuleLogicHandler::getParameters() const
{
    return m_parameters;
}
