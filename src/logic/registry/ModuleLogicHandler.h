#pragma once

#include <QObject>
#include <QString>

#include "contracts/UiAction.h"
#include "contracts/LogicNotification.h"

class SceneGraph;

class ModuleLogicHandler : public QObject
{
    Q_OBJECT

public:
    explicit ModuleLogicHandler(const QString& moduleId, QObject* parent = nullptr);

    QString getModuleId() const;
    void setSceneGraph(SceneGraph* scene);
    SceneGraph* getSceneGraph() const;

    virtual void handleAction(const UiAction& action) = 0;

    virtual void onModuleActivated() {}
    virtual void onModuleDeactivated() {}
    virtual void onResync() {}

signals:
    void logicNotification(const LogicNotification& notification);

private:
    const QString m_moduleId;
    SceneGraph* m_sceneGraph = nullptr;
};
