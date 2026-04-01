#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QUuid>
#include <QDateTime>

struct LogicNotification
{
    enum EventType {
        ModuleChanged,
        StageChanged,
        ButtonStateChanged,
        TipChanged,
        SceneNodesUpdated,
        WorkflowChanged,
        ConnectionStateChanged,
        ErrorOccurred,
        CustomEvent
    };

    enum Level {
        Info,
        Warning,
        Error
    };

    enum TargetScope {
        Shell,
        CurrentModule,
        AllModules,
        ModuleList
    };

    QString      notificationId;
    QString      sourceActionId;
    EventType    eventType;
    Level        level;
    qint64       timestampMs;
    TargetScope  targetScope;
    QStringList  targetModules;
    QVariantMap  payload;

    static LogicNotification create(EventType type,
                                    TargetScope scope,
                                    const QVariantMap& payload = {})
    {
        LogicNotification n;
        n.notificationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        n.sourceActionId = QString();
        n.eventType      = type;
        n.level          = Info;
        n.timestampMs    = QDateTime::currentMSecsSinceEpoch();
        n.targetScope    = scope;
        n.targetModules  = QStringList();
        n.payload        = payload;
        return n;
    }

    void setSourceActionId(const QString& id) { sourceActionId = id; }
    void setLevel(Level lvl)                  { level = lvl; }
};
