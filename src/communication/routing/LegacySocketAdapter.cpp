#include "LegacySocketAdapter.h"

namespace {

const QString& heartbeatType()
{
    static const QString value = QStringLiteral("heartbeat");
    return value;
}

const QString& actionType()
{
    static const QString value = QStringLiteral("action");
    return value;
}

const QString& actionRequestType()
{
    static const QString value = QStringLiteral("action_request");
    return value;
}

const QString& uiActionType()
{
    static const QString value = QStringLiteral("ui_action");
    return value;
}

const QString& commandType()
{
    static const QString value = QStringLiteral("command");
    return value;
}

const QString& serverCommandType()
{
    static const QString value = QStringLiteral("server_command");
    return value;
}

const QString& resyncRequestType()
{
    static const QString value = QStringLiteral("resync_request");
    return value;
}

const QString& resyncResponseType()
{
    static const QString value = QStringLiteral("resync_response");
    return value;
}

QString normalizeMessageType(const QString& value)
{
    return value.trimmed().toLower().replace(QLatin1Char('-'), QLatin1Char('_'));
}

QVariantMap objectPayload(const QVariantMap& envelope)
{
    QVariantMap value = envelope.value(QStringLiteral("value")).toMap();
    if (!value.isEmpty()) {
        return value;
    }

    value = envelope.value(QStringLiteral("payload")).toMap();
    if (!value.isEmpty()) {
        return value;
    }

    return envelope;
}

} // namespace

bool LegacySocketEnvelope::isGlobalTarget() const
{
    return module.trimmed().compare(QStringLiteral("global"), Qt::CaseInsensitive) == 0;
}

bool LegacySocketEnvelope::isHeartbeat() const
{
    return type == heartbeatType();
}

bool LegacySocketEnvelope::isControlMessage() const
{
    return type == actionType() ||
        type == actionRequestType() ||
        type == uiActionType();
}

bool LegacySocketEnvelope::isServerCommand() const
{
    return type == commandType() ||
        type == serverCommandType();
}

bool LegacySocketEnvelope::isResyncMessage() const
{
    return type == resyncRequestType() ||
        type == resyncResponseType();
}

QString LegacySocketEnvelope::commandType() const
{
    QString command = payload.value(QStringLiteral("commandType")).toString();
    if (command.isEmpty()) {
        command = payload.value(QStringLiteral("command")).toString();
    }
    return command;
}

StateSample LegacySocketEnvelope::toStateSample() const
{
    return StateSample::create(
        QStringLiteral("socket"),
        isGlobalTarget() ? QStringLiteral("global") : module,
        type,
        payload);
}

LegacySocketEnvelope LegacySocketAdapter::fromEnvelope(const QVariantMap& envelope)
{
    LegacySocketEnvelope message;
    message.module = envelope.value(QStringLiteral("module")).toString().trimmed();
    message.type = normalizeMessageType(envelope.value(QStringLiteral("type")).toString());
    message.payload = objectPayload(envelope);

    if (message.type.isEmpty()) {
        message.errorCode = QStringLiteral("SOCKET_MESSAGE_TYPE_MISSING");
        message.errorMessage = QStringLiteral("Socket envelope is missing type");
        return message;
    }

    if (!message.payload.contains(QStringLiteral("module")) && !message.module.isEmpty()) {
        message.payload.insert(QStringLiteral("module"), message.module);
    }

    return message;
}

QVariantMap LegacySocketAdapter::toEnvelope(const QString& module,
                                            const QString& type,
                                            const QVariantMap& payload)
{
    QVariantMap envelope;
    envelope.insert(QStringLiteral("module"), module);
    envelope.insert(QStringLiteral("type"), type);
    envelope.insert(QStringLiteral("value"), payload);
    return envelope;
}
