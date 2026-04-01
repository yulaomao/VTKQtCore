#include "NavigationModuleLogicHandler.h"

#include "logic/scene/SceneGraph.h"
#include "logic/scene/nodes/TransformNode.h"

NavigationModuleLogicHandler::NavigationModuleLogicHandler(QObject* parent)
    : ModuleLogicHandler(QStringLiteral("navigation"), parent)
{
}

void NavigationModuleLogicHandler::onModuleActivated()
{
    SceneGraph* scene = getSceneGraph();
    if (!scene)
        return;

    auto* transformNode = new TransformNode(scene);
    transformNode->setTransformKind(QStringLiteral("tool_tracking"));
    transformNode->setShowAxes(true);
    scene->addNode(transformNode);
    m_toolTransformNodeId = transformNode->getNodeId();

    QVariantMap payload;
    payload.insert(QStringLiteral("toolTransformNodeId"), m_toolTransformNodeId);
    emit logicNotification(LogicNotification::create(
        LogicNotification::SceneNodesUpdated,
        LogicNotification::CurrentModule,
        payload));
}

void NavigationModuleLogicHandler::handleAction(const UiAction& action)
{
    if (action.actionType == UiAction::StartNavigation) {
        m_navigating = true;

        QVariantMap payload;
        payload.insert(QStringLiteral("navigating"), true);
        payload.insert(QStringLiteral("toolTransformNodeId"), m_toolTransformNodeId);

        auto notification = LogicNotification::create(
            LogicNotification::StageChanged,
            LogicNotification::CurrentModule,
            payload);
        notification.setSourceActionId(action.actionId);
        emit logicNotification(notification);
        return;
    }

    if (action.actionType == UiAction::StopNavigation) {
        m_navigating = false;

        QVariantMap payload;
        payload.insert(QStringLiteral("navigating"), false);
        payload.insert(QStringLiteral("toolTransformNodeId"), m_toolTransformNodeId);

        auto notification = LogicNotification::create(
            LogicNotification::StageChanged,
            LogicNotification::CurrentModule,
            payload);
        notification.setSourceActionId(action.actionId);
        emit logicNotification(notification);
    }
}

void NavigationModuleLogicHandler::onModuleDeactivated()
{
}

void NavigationModuleLogicHandler::onResync()
{
    QVariantMap payload;
    payload.insert(QStringLiteral("navigating"), m_navigating);
    payload.insert(QStringLiteral("toolTransformNodeId"), m_toolTransformNodeId);
    emit logicNotification(LogicNotification::create(
        LogicNotification::StageChanged,
        LogicNotification::CurrentModule,
        payload));
}
