#include "PlanningModuleLogicHandler.h"

#include "logic/scene/SceneGraph.h"
#include "logic/scene/nodes/ModelNode.h"
#include "logic/scene/nodes/LineNode.h"

PlanningModuleLogicHandler::PlanningModuleLogicHandler(QObject* parent)
    : ModuleLogicHandler(QStringLiteral("planning"), parent)
{
}

void PlanningModuleLogicHandler::onModuleActivated()
{
    SceneGraph* scene = getSceneGraph();
    if (!scene)
        return;

    auto* modelNode = new ModelNode(scene);
    modelNode->setModelRole(QStringLiteral("plan_model"));
    scene->addNode(modelNode);
    m_planModelNodeId = modelNode->getNodeId();

    auto* lineNode = new LineNode(scene);
    lineNode->setLineRole(QStringLiteral("plan_path"));
    scene->addNode(lineNode);
    m_planPathNodeId = lineNode->getNodeId();

    QVariantMap payload;
    payload.insert(QStringLiteral("planModelNodeId"), m_planModelNodeId);
    payload.insert(QStringLiteral("planPathNodeId"), m_planPathNodeId);
    emit logicNotification(LogicNotification::create(
        LogicNotification::SceneNodesUpdated,
        LogicNotification::CurrentModule,
        payload));
}

void PlanningModuleLogicHandler::handleAction(const UiAction& action)
{
    if (action.actionType != UiAction::CustomAction)
        return;

    const QString subType = action.payload.value(QStringLiteral("subType")).toString();

    if (subType == QStringLiteral("generate_plan")) {
        QVariantMap payload;
        payload.insert(QStringLiteral("planModelNodeId"), m_planModelNodeId);
        payload.insert(QStringLiteral("planPathNodeId"), m_planPathNodeId);
        payload.insert(QStringLiteral("status"), QStringLiteral("generated"));

        auto notification = LogicNotification::create(
            LogicNotification::SceneNodesUpdated,
            LogicNotification::CurrentModule,
            payload);
        notification.setSourceActionId(action.actionId);
        emit logicNotification(notification);
    } else if (subType == QStringLiteral("accept_plan")) {
        QVariantMap payload;
        payload.insert(QStringLiteral("planModelNodeId"), m_planModelNodeId);
        payload.insert(QStringLiteral("planPathNodeId"), m_planPathNodeId);
        payload.insert(QStringLiteral("status"), QStringLiteral("accepted"));

        auto notification = LogicNotification::create(
            LogicNotification::StageChanged,
            LogicNotification::CurrentModule,
            payload);
        notification.setSourceActionId(action.actionId);
        emit logicNotification(notification);
    }
}

void PlanningModuleLogicHandler::onModuleDeactivated()
{
}

void PlanningModuleLogicHandler::onResync()
{
    QVariantMap payload;
    payload.insert(QStringLiteral("planModelNodeId"), m_planModelNodeId);
    payload.insert(QStringLiteral("planPathNodeId"), m_planPathNodeId);
    emit logicNotification(LogicNotification::create(
        LogicNotification::SceneNodesUpdated,
        LogicNotification::CurrentModule,
        payload));
}
