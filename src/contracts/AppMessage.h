#pragma once

#include <QDateTime>
#include <QMetaType>
#include <QString>
#include <QVariantMap>
#include <QUuid>

struct TargetAddress
{
    QString name;

    static TargetAddress module(const QString& moduleId)
    {
        return {moduleId.trimmed()};
    }

    static TargetAddress global()
    {
        return {QStringLiteral("global")};
    }

    static TargetAddress shell()
    {
        return {QStringLiteral("shell")};
    }

    static TargetAddress globalUi()
    {
        return {QStringLiteral("global-ui")};
    }

    bool isGlobal() const
    {
        return name.compare(QStringLiteral("global"), Qt::CaseInsensitive) == 0;
    }

    bool isShell() const
    {
        return name.compare(QStringLiteral("shell"), Qt::CaseInsensitive) == 0;
    }

    bool isGlobalUi() const
    {
        return name.compare(QStringLiteral("global-ui"), Qt::CaseInsensitive) == 0;
    }
};

struct ErrorInfo
{
    QString code;
    QString message;
    bool recoverable = true;
    QVariantMap context;
};

enum class AppMessageKind
{
    ExternalData,
    UiIntent,
    ModuleEvent,
    OutboundCommand,
    ShellEvent,
    GlobalUiEvent,
    Error
};

struct AppMessage
{
    QString messageId;
    QString correlationId;
    QString source;
    TargetAddress target;
    AppMessageKind kind = AppMessageKind::ExternalData;
    QString eventName;
    qint64 timestampMs = 0;
    QVariantMap payload;
    ErrorInfo error;

    static AppMessage create(AppMessageKind kind,
                             const TargetAddress& target,
                             const QVariantMap& payload = {},
                             const QString& eventName = QString(),
                             const QString& source = QString())
    {
        AppMessage message;
        message.messageId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        message.correlationId = message.messageId;
        message.source = source;
        message.target = target;
        message.kind = kind;
        message.eventName = eventName.trimmed();
        message.timestampMs = QDateTime::currentMSecsSinceEpoch();
        message.payload = payload;
        return message;
    }

    static QString kindToString(AppMessageKind kind)
    {
        switch (kind) {
        case AppMessageKind::ExternalData:
            return QStringLiteral("external_data");
        case AppMessageKind::UiIntent:
            return QStringLiteral("ui_intent");
        case AppMessageKind::ModuleEvent:
            return QStringLiteral("module_event");
        case AppMessageKind::OutboundCommand:
            return QStringLiteral("outbound_command");
        case AppMessageKind::ShellEvent:
            return QStringLiteral("shell_event");
        case AppMessageKind::GlobalUiEvent:
            return QStringLiteral("global_ui_event");
        case AppMessageKind::Error:
            return QStringLiteral("error");
        }

        return QStringLiteral("unknown");
    }

    static AppMessageKind kindFromString(const QString& value)
    {
        QString normalized = value.trimmed().toLower();
        normalized.replace(QLatin1Char('-'), QLatin1Char('_'));

        if (normalized == QStringLiteral("ui_intent")) {
            return AppMessageKind::UiIntent;
        }
        if (normalized == QStringLiteral("module_event")) {
            return AppMessageKind::ModuleEvent;
        }
        if (normalized == QStringLiteral("outbound_command")) {
            return AppMessageKind::OutboundCommand;
        }
        if (normalized == QStringLiteral("shell_event")) {
            return AppMessageKind::ShellEvent;
        }
        if (normalized == QStringLiteral("global_ui_event")) {
            return AppMessageKind::GlobalUiEvent;
        }
        if (normalized == QStringLiteral("error")) {
            return AppMessageKind::Error;
        }

        return AppMessageKind::ExternalData;
    }
};

Q_DECLARE_METATYPE(AppMessage)
