#include "PointPickModuleLogicHandler.h"

#include "PointPickUiCommands.h"
#include "logic/scene/SceneGraph.h"
#include "logic/scene/nodes/PointNode.h"

#include <array>
#include <QStringList>

namespace {

QVariantMap normalizedSampleData(const StateSample& sample)
{
    const QVariantMap values = sample.data.value(QStringLiteral("values")).toMap();
    if (!values.isEmpty()) {
        QVariantMap aggregatedPayload;
        for (auto it = values.cbegin(); it != values.cend(); ++it) {
            const QVariantMap entryPayload = it.value().toMap();
            if (entryPayload.isEmpty()) {
                continue;
            }
            for (auto entryIt = entryPayload.cbegin(); entryIt != entryPayload.cend(); ++entryIt) {
                aggregatedPayload.insert(entryIt.key(), entryIt.value());
            }
        }
        return aggregatedPayload;
    }

    return sample.data;
}

bool extractPointPosition(const QVariant& value, std::array<double, 3>& position)
{
    const QVariantMap pointMap = value.toMap();
    if (!pointMap.isEmpty()) {
        if (pointMap.contains(QStringLiteral("position"))) {
            return extractPointPosition(pointMap.value(QStringLiteral("position")), position);
        }

        if (pointMap.contains(QStringLiteral("x")) &&
            pointMap.contains(QStringLiteral("y")) &&
            pointMap.contains(QStringLiteral("z"))) {
            position = {
                pointMap.value(QStringLiteral("x")).toDouble(),
                pointMap.value(QStringLiteral("y")).toDouble(),
                pointMap.value(QStringLiteral("z")).toDouble()
            };
            return true;
        }
    }

    const QVariantList coords = value.toList();
    if (coords.size() < 3) {
        return false;
    }

    position = {
        coords.at(0).toDouble(),
        coords.at(1).toDouble(),
        coords.at(2).toDouble()
    };
    return true;
}

QVector<PointItem> extractPoints(const QVariantMap& payload)
{
    QVector<PointItem> points;
    const QVariantList pointList = payload.value(QStringLiteral("points")).toList();
    points.reserve(pointList.size());

    for (const QVariant& entry : pointList) {
        std::array<double, 3> position = {0.0, 0.0, 0.0};
        if (!extractPointPosition(entry, position)) {
            continue;
        }

        const QVariantMap pointMap = entry.toMap();
        PointItem item;
        item.pointId = pointMap.value(QStringLiteral("pointId")).toString();
        item.label = pointMap.value(QStringLiteral("label")).toString();
        item.selectedFlag = pointMap.value(QStringLiteral("selected")).toBool();
        item.visibleFlag = pointMap.value(QStringLiteral("visible"), true).toBool();
        item.lockedFlag = pointMap.value(QStringLiteral("locked")).toBool();
        item.position[0] = position[0];
        item.position[1] = position[1];
        item.position[2] = position[2];
        points.append(item);
    }

    return points;
}

QStringList buildPointSummary(const PointNode* pointNode)
{
    QStringList points;
    if (!pointNode) {
        return points;
    }

    const int selectedIndex = pointNode->getSelectedPointIndex();
    for (int index = 0; index < pointNode->getPointCount(); ++index) {
        const PointItem& point = pointNode->getPointByIndex(index);
        QString label = point.label;
        if (label.isEmpty()) {
            label = point.pointId.isEmpty()
                ? QStringLiteral("Point %1").arg(index + 1)
                : QStringLiteral("Point %1").arg(point.pointId.left(8));
        }

        QString entry = QStringLiteral("%1 (%2, %3, %4)")
                            .arg(label)
                            .arg(point.position[0], 0, 'f', 1)
                            .arg(point.position[1], 0, 'f', 1)
                            .arg(point.position[2], 0, 'f', 1);
        if (index == selectedIndex || point.selectedFlag) {
            entry.prepend(QStringLiteral("[selected] "));
        }
        points.append(entry);
    }

    return points;
}

} // namespace

PointPickModuleLogicHandler::PointPickModuleLogicHandler(QObject* parent)
    : ModuleLogicHandler(QStringLiteral("pointpick"), parent)
{
}

void PointPickModuleLogicHandler::onModuleActivated()
{
    if (!ensureSelectionNode(getSceneGraph())) {
        return;
    }

    emitSelectionState();
}

void PointPickModuleLogicHandler::handleAction(const UiAction& action)
{
    if (action.actionType != UiAction::CustomAction) {
        return;
    }

    const QString command = action.payload.value(QStringLiteral("command")).toString().trimmed();

    if (command == PointPickUiCommands::confirmPoints()) {
        if (!ensureSelectionNode(getSceneGraph())) {
            return;
        }

        m_confirmed = true;
        emitSelectionState(action.actionId);
        return;
    }

    const QString subType = action.payload.value(QStringLiteral("subType")).toString();
    if (subType != QStringLiteral("add_point")) {
        return;
    }

    auto* pointNode = ensureSelectionNode(getSceneGraph());
    if (!pointNode) {
        return;
    }

    PointItem item;
    item.position[0] = action.payload.value(QStringLiteral("x")).toDouble();
    item.position[1] = action.payload.value(QStringLiteral("y")).toDouble();
    item.position[2] = action.payload.value(QStringLiteral("z")).toDouble();
    pointNode->addPoint(item);

    emitSelectionState(action.actionId);
}

void PointPickModuleLogicHandler::handleStateSample(const StateSample& sample)
{
    SceneGraph* scene = getSceneGraph();
    auto* pointNode = ensureSelectionNode(scene);
    if (!pointNode) {
        return;
    }

    const QVariantMap payloadData = normalizedSampleData(sample);
    bool changed = false;

    if (payloadData.contains(QStringLiteral("points"))) {
        const QVector<PointItem> points = extractPoints(payloadData);
        scene->startBatchModify();
        pointNode->removeAllPoints();
        for (const PointItem& point : points) {
            pointNode->addPoint(point);
        }
        scene->endBatchModify();
        changed = true;
    } else {
        std::array<double, 3> position = {0.0, 0.0, 0.0};
        if (extractPointPosition(payloadData, position)) {
            PointItem item;
            item.label = payloadData.value(QStringLiteral("label")).toString();
            item.selectedFlag = payloadData.value(QStringLiteral("selected")).toBool();
            item.position[0] = position[0];
            item.position[1] = position[1];
            item.position[2] = position[2];
            pointNode->addPoint(item);
            changed = true;
        }
    }

    if (payloadData.contains(QStringLiteral("confirmed"))) {
        m_confirmed = payloadData.value(QStringLiteral("confirmed")).toBool();
        changed = true;
    }

    if (payloadData.contains(QStringLiteral("selectedIndex"))) {
        pointNode->setSelectedPointIndex(payloadData.value(QStringLiteral("selectedIndex")).toInt());
        changed = true;
    }

    if (changed) {
        emitSelectionState(sample.sampleId);
    }
}

void PointPickModuleLogicHandler::onModuleDeactivated()
{
}

void PointPickModuleLogicHandler::onResync()
{
    emitSelectionState();
}

PointNode* PointPickModuleLogicHandler::ensureSelectionNode(SceneGraph* scene)
{
    if (!scene) {
        return nullptr;
    }

    if (!m_pointNodeId.isEmpty()) {
        if (auto* existing = scene->getNodeById<PointNode>(m_pointNodeId)) {
            return existing;
        }
        m_pointNodeId.clear();
    }

    if (auto* existing = findSelectionNode(scene)) {
        m_pointNodeId = existing->getNodeId();
        return existing;
    }

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
    return pointNode;
}

PointNode* PointPickModuleLogicHandler::findSelectionNode(SceneGraph* scene) const
{
    if (!scene) {
        return nullptr;
    }

    const QVector<PointNode*> pointNodes = scene->getAllPointNodes();
    for (PointNode* pointNode : pointNodes) {
        if (pointNode && pointNode->getPointRole() == QStringLiteral("selection_points")) {
            return pointNode;
        }
    }

    return nullptr;
}

void PointPickModuleLogicHandler::emitSelectionState(const QString& sourceActionId)
{
    QVariantMap payload;
    payload.insert(QStringLiteral("pointNodeId"), m_pointNodeId);
    payload.insert(QStringLiteral("confirmed"), m_confirmed);

    SceneGraph* scene = getSceneGraph();
    if (scene) {
        if (auto* pointNode = scene->getNodeById<PointNode>(m_pointNodeId)) {
            payload.insert(QStringLiteral("pointCount"), pointNode->getPointCount());
            payload.insert(QStringLiteral("selectedIndex"), pointNode->getSelectedPointIndex());
            payload.insert(QStringLiteral("points"), buildPointSummary(pointNode));
        }
    }

    LogicNotification notification = LogicNotification::create(
        LogicNotification::SceneNodesUpdated,
        LogicNotification::CurrentModule,
        payload);
    notification.setSourceActionId(sourceActionId);
    emit logicNotification(notification);
}
