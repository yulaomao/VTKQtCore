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
