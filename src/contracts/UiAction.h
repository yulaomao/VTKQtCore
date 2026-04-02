#pragma once

#include <QString>
#include <QVariantMap>
#include <QUuid>
#include <QDateTime>

struct UiAction
{
    enum ActionType {
        NextStep,
        PrevStep,
        RequestSwitchModule,
        ConfirmPoints,
        GeneratePlan,
        AcceptPlan,
        StartNavigation,
        StopNavigation,
        UpdateParameter,
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
        case NextStep:
            return QStringLiteral("next_step");
        case PrevStep:
            return QStringLiteral("prev_step");
        case RequestSwitchModule:
            return QStringLiteral("request_switch_module");
        case ConfirmPoints:
            return QStringLiteral("confirm_points");
        case GeneratePlan:
            return QStringLiteral("generate_plan");
        case AcceptPlan:
            return QStringLiteral("accept_plan");
        case StartNavigation:
            return QStringLiteral("start_navigation");
        case StopNavigation:
            return QStringLiteral("stop_navigation");
        case UpdateParameter:
            return QStringLiteral("update_parameter");
        case CustomAction:
        default:
            return QStringLiteral("custom_action");
        }
    }

    static bool fromString(const QString& rawType, ActionType& type)
    {
        QString normalized = rawType.trimmed().toLower();
        normalized.replace(QLatin1Char('-'), QLatin1Char('_'));

        if (normalized == QStringLiteral("next_step")) {
            type = NextStep;
            return true;
        }
        if (normalized == QStringLiteral("prev_step") ||
            normalized == QStringLiteral("previous_step")) {
            type = PrevStep;
            return true;
        }
        if (normalized == QStringLiteral("request_switch_module") ||
            normalized == QStringLiteral("switch_module")) {
            type = RequestSwitchModule;
            return true;
        }
        if (normalized == QStringLiteral("confirm_points")) {
            type = ConfirmPoints;
            return true;
        }
        if (normalized == QStringLiteral("generate_plan")) {
            type = GeneratePlan;
            return true;
        }
        if (normalized == QStringLiteral("accept_plan")) {
            type = AcceptPlan;
            return true;
        }
        if (normalized == QStringLiteral("start_navigation")) {
            type = StartNavigation;
            return true;
        }
        if (normalized == QStringLiteral("stop_navigation")) {
            type = StopNavigation;
            return true;
        }
        if (normalized == QStringLiteral("update_parameter")) {
            type = UpdateParameter;
            return true;
        }
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
