#pragma once

#include <QObject>

#include "contracts/UiAction.h"
#include "contracts/LogicNotification.h"

class SceneGraph;
class WorkflowStateMachine;
class ModuleLogicRegistry;
class ModuleLogicHandler;

class LogicRuntime : public QObject
{
    Q_OBJECT

public:
    explicit LogicRuntime(QObject* parent = nullptr);

    SceneGraph* getSceneGraph() const;
    WorkflowStateMachine* getWorkflowStateMachine() const;
    ModuleLogicRegistry* getModuleLogicRegistry() const;

    void registerModuleHandler(ModuleLogicHandler* handler);

public slots:
    void onActionReceived(const UiAction& action);

signals:
    void logicNotification(const LogicNotification& notification);

private:
    void switchToModule(const QString& targetModule, const QString& sourceActionId);
    void routeToModuleHandler(const UiAction& action);

    SceneGraph* m_sceneGraph;
    WorkflowStateMachine* m_workflowStateMachine;
    ModuleLogicRegistry* m_moduleLogicRegistry;
};
