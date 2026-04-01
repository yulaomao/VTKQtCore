#include "LogicRuntime.h"

#include "scene/SceneGraph.h"
#include "workflow/WorkflowStateMachine.h"
#include "registry/ModuleLogicRegistry.h"
#include "registry/ModuleLogicHandler.h"

LogicRuntime::LogicRuntime(QObject* parent)
    : QObject(parent)
    , m_sceneGraph(new SceneGraph(this))
    , m_workflowStateMachine(new WorkflowStateMachine(this))
    , m_moduleLogicRegistry(new ModuleLogicRegistry(this))
{
}

SceneGraph* LogicRuntime::getSceneGraph() const
{
    return m_sceneGraph;
}

WorkflowStateMachine* LogicRuntime::getWorkflowStateMachine() const
{
    return m_workflowStateMachine;
}

ModuleLogicRegistry* LogicRuntime::getModuleLogicRegistry() const
{
    return m_moduleLogicRegistry;
}

void LogicRuntime::registerModuleHandler(ModuleLogicHandler* handler)
{
    if (!handler) {
        return;
    }

    handler->setSceneGraph(m_sceneGraph);
    m_moduleLogicRegistry->registerHandler(handler);

    connect(handler, &ModuleLogicHandler::logicNotification,
            this, &LogicRuntime::logicNotification);
}

void LogicRuntime::onActionReceived(const UiAction& action)
{
    switch (action.actionType) {
    case UiAction::NextStep: {
        if (m_workflowStateMachine->canAdvanceToNext()) {
            switchToModule(m_workflowStateMachine->getNextModule(), action.actionId);
        } else {
            LogicNotification notification = LogicNotification::create(
                LogicNotification::ErrorOccurred,
                LogicNotification::Shell,
                {{QStringLiteral("message"), QStringLiteral("Cannot advance to next step")}});
            notification.setSourceActionId(action.actionId);
            notification.setLevel(LogicNotification::Warning);
            emit logicNotification(notification);
        }
        break;
    }
    case UiAction::PrevStep: {
        if (m_workflowStateMachine->canGoToPrev()) {
            switchToModule(m_workflowStateMachine->getPrevModule(), action.actionId);
        } else {
            LogicNotification notification = LogicNotification::create(
                LogicNotification::ErrorOccurred,
                LogicNotification::Shell,
                {{QStringLiteral("message"), QStringLiteral("Cannot go to previous step")}});
            notification.setSourceActionId(action.actionId);
            notification.setLevel(LogicNotification::Warning);
            emit logicNotification(notification);
        }
        break;
    }
    case UiAction::RequestSwitchModule: {
        const QString targetModule = action.payload.value(QStringLiteral("targetModule")).toString();
        if (m_workflowStateMachine->isModuleEnterable(targetModule)) {
            switchToModule(targetModule, action.actionId);
        } else {
            LogicNotification notification = LogicNotification::create(
                LogicNotification::ErrorOccurred,
                LogicNotification::Shell,
                {{QStringLiteral("message"),
                  QStringLiteral("Module '%1' is not enterable").arg(targetModule)}});
            notification.setSourceActionId(action.actionId);
            notification.setLevel(LogicNotification::Warning);
            emit logicNotification(notification);
        }
        break;
    }
    default:
        routeToModuleHandler(action);
        break;
    }
}

void LogicRuntime::switchToModule(const QString& targetModule, const QString& sourceActionId)
{
    const QString oldModule = m_workflowStateMachine->getCurrentModule();

    // Deactivate old module handler
    if (!oldModule.isEmpty()) {
        ModuleLogicHandler* oldHandler = m_moduleLogicRegistry->getHandler(oldModule);
        if (oldHandler) {
            oldHandler->onModuleDeactivated();
        }
    }

    // Update workflow state
    m_workflowStateMachine->setCurrentModule(targetModule);

    // Activate new module handler
    ModuleLogicHandler* newHandler = m_moduleLogicRegistry->getHandler(targetModule);
    if (newHandler) {
        newHandler->onModuleActivated();
    }

    // Emit ModuleChanged notification with Shell scope
    LogicNotification notification = LogicNotification::create(
        LogicNotification::ModuleChanged,
        LogicNotification::Shell,
        {{QStringLiteral("newModule"), targetModule},
         {QStringLiteral("oldModule"), oldModule}});
    notification.setSourceActionId(sourceActionId);
    emit logicNotification(notification);
}

void LogicRuntime::routeToModuleHandler(const UiAction& action)
{
    const QString currentModule = m_workflowStateMachine->getCurrentModule();
    ModuleLogicHandler* handler = m_moduleLogicRegistry->getHandler(currentModule);
    if (handler) {
        handler->handleAction(action);
    }
}
