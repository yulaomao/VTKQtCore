#include "PointPickModuleLogicHandler.h"

#include "logic/scene/SceneGraph.h"
#include "logic/scene/nodes/PointNode.h"

PointPickModuleLogicHandler::PointPickModuleLogicHandler(QObject* parent)
    : ModuleLogicHandler(QStringLiteral("pointpick"), parent)
{
}

void PointPickModuleLogicHandler::onModuleActivated()
{
    SceneGraph* scene = getSceneGraph();
    if (!scene)
        return;

    auto* pointNode = new PointNode(scene);
    pointNode->setPointRole(QStringLiteral("selection_points"));

    const double red[4] = {1.0, 0.0, 0.0, 1.0};
    pointNode->setDefaultPointColor(red);
    pointNode->setDefaultPointSize(5.0);

    DisplayTarget dt;
    dt.visible = true;
    dt.layer = 3;
    pointNode->setDefaultDisplayTarget(dt);

    scene->addNode(pointNode);
    m_pointNodeId = pointNode->getNodeId();

    QVariantMap payload;
    payload.insert(QStringLiteral("pointNodeId"), m_pointNodeId);
    emit logicNotification(LogicNotification::create(
        LogicNotification::SceneNodesUpdated,
        LogicNotification::CurrentModule,
        payload));
}

void PointPickModuleLogicHandler::handleAction(const UiAction& action)
{
    if (action.actionType == UiAction::ConfirmPoints) {
        m_confirmed = true;

        QVariantMap payload;
        payload.insert(QStringLiteral("confirmed"), true);
        payload.insert(QStringLiteral("pointNodeId"), m_pointNodeId);

        auto notification = LogicNotification::create(
            LogicNotification::SceneNodesUpdated,
            LogicNotification::CurrentModule,
            payload);
        notification.setSourceActionId(action.actionId);
        emit logicNotification(notification);
        return;
    }

    if (action.actionType == UiAction::CustomAction) {
        const QString subType = action.payload.value(QStringLiteral("subType")).toString();
        if (subType == QStringLiteral("add_point")) {
            SceneGraph* scene = getSceneGraph();
            if (!scene)
                return;

            auto* pointNode = scene->getNodeById<PointNode>(m_pointNodeId);
            if (!pointNode)
                return;

            PointItem item;
            item.position[0] = action.payload.value(QStringLiteral("x")).toDouble();
            item.position[1] = action.payload.value(QStringLiteral("y")).toDouble();
            item.position[2] = action.payload.value(QStringLiteral("z")).toDouble();
            pointNode->addPoint(item);

            QVariantMap payload;
            payload.insert(QStringLiteral("pointNodeId"), m_pointNodeId);
            payload.insert(QStringLiteral("pointCount"), pointNode->getPointCount());

            auto notification = LogicNotification::create(
                LogicNotification::SceneNodesUpdated,
                LogicNotification::CurrentModule,
                payload);
            notification.setSourceActionId(action.actionId);
            emit logicNotification(notification);
        }
    }
}

void PointPickModuleLogicHandler::onModuleDeactivated()
{
}

void PointPickModuleLogicHandler::onResync()
{
    QVariantMap payload;
    payload.insert(QStringLiteral("pointNodeId"), m_pointNodeId);
    payload.insert(QStringLiteral("confirmed"), m_confirmed);
    emit logicNotification(LogicNotification::create(
        LogicNotification::SceneNodesUpdated,
        LogicNotification::CurrentModule,
        payload));
}
