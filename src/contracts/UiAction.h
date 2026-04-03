#pragma once

#include <QString>
#include <QVariantMap>
#include <QUuid>
#include <QDateTime>

struct UiAction
{
    enum ActionType {
        CustomAction
    };

    QString      actionId;
    ActionType   actionType;
    QString      module;
    qint64       timestampMs;
    QVariantMap  payload;

    static QString toString(ActionType type)
    {
        switch (type) {
        case CustomAction:
        default:
            return QStringLiteral("custom_action");
        }
    }

    static bool fromString(const QString& rawType, ActionType& type)
    {
        QString normalized = rawType.trimmed().toLower();
        normalized.replace(QLatin1Char('-'), QLatin1Char('_'));

        if (normalized == QStringLiteral("custom_action")) {
            type = CustomAction;
            return true;
        }

        return false;
    }

    static UiAction create(ActionType type,
                           const QString& module,
                           const QVariantMap& payload = {})
    {
        UiAction a;
        a.actionId    = QUuid::createUuid().toString(QUuid::WithoutBraces);
        a.actionType  = type;
        a.module      = module;
        a.timestampMs = QDateTime::currentMSecsSinceEpoch();
        a.payload     = payload;
        return a;
    }
};
