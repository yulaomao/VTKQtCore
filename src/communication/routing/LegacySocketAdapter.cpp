#include "LegacySocketAdapter.h"

namespace {

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
    return type == QStringLiteral("heartbeat");
}

bool LegacySocketEnvelope::isControlMessage() const
{
    return type == QStringLiteral("action") ||
        type == QStringLiteral("action_request") ||
        type == QStringLiteral("ui_action");
}

bool LegacySocketEnvelope::isServerCommand() const
{
    return type == QStringLiteral("command") ||
        type == QStringLiteral("server_command");
}

bool LegacySocketEnvelope::isResyncMessage() const
{
    return type == QStringLiteral("resync_request") ||
        type == QStringLiteral("resync_response");
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
