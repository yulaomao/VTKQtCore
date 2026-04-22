#include "PlanningModuleLogicHandler.h"

#include "PlanningUiCommands.h"

#include "logic/scene/SceneGraph.h"
#include "logic/scene/nodes/BillboardArrowNode.h"
#include "logic/scene/nodes/BillboardLineNode.h"
#include "logic/scene/nodes/LineNode.h"
#include "logic/scene/nodes/ModelNode.h"

#include <algorithm>
#include <array>
#include <QByteArray>
#include <QJsonDocument>

namespace {

constexpr char kPlanPathSegmentRole[] = "plan_path_segment";
constexpr char kPlanPathArrowRole[] = "plan_path_arrow";

QVector<std::array<double, 3>> createPlanVertices()
{
    return {
        {-40.0, -20.0, 0.0},
        {40.0, -20.0, 0.0},
        {0.0, 50.0, 18.0},
        {0.0, 0.0, 72.0}
    };
}

QVector<std::array<int, 3>> createPlanTriangles()
{
    return {
        {0, 1, 2},
        {0, 1, 3},
        {1, 2, 3},
        {2, 0, 3}
    };
}

QVector<std::array<double, 3>> createPlanPath()
{
    return {
        {0.0, 0.0, 90.0},
        {0.0, 0.0, 55.0},
        {0.0, 10.0, 25.0},
        {0.0, 14.0, 6.0}
    };
}

QVariantMap decodeJsonValue(const QVariant& value)
{
    const QByteArray bytes = value.toByteArray();
    if (!bytes.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(bytes);
        if (doc.isObject()) {
            return doc.object().toVariantMap();
        }
    }
    return value.toMap();
}

bool extractVec3(const QVariant& value, std::array<double, 3>& result)
{
    const QVariantMap map = value.toMap();
    if (!map.isEmpty()) {
        if (map.contains(QStringLiteral("position"))) {
            return extractVec3(map.value(QStringLiteral("position")), result);
        }
        if (map.contains(QStringLiteral("x")) &&
            map.contains(QStringLiteral("y")) &&
            map.contains(QStringLiteral("z"))) {
            result = {
                map.value(QStringLiteral("x")).toDouble(),
                map.value(QStringLiteral("y")).toDouble(),
                map.value(QStringLiteral("z")).toDouble()
            };
            return true;
        }
    }

    const QVariantList list = value.toList();
    if (list.size() < 3) {
        return false;
    }

    result = {list.at(0).toDouble(), list.at(1).toDouble(), list.at(2).toDouble()};
    return true;
}

bool extractTriangle(const QVariant& value, std::array<int, 3>& triangle)
{
    const QVariantMap map = value.toMap();
    if (!map.isEmpty()) {
        if (map.contains(QStringLiteral("indices"))) {
            return extractTriangle(map.value(QStringLiteral("indices")), triangle);
        }
        if (map.contains(QStringLiteral("a")) &&
            map.contains(QStringLiteral("b")) &&
            map.contains(QStringLiteral("c"))) {
            triangle = {
                map.value(QStringLiteral("a")).toInt(),
                map.value(QStringLiteral("b")).toInt(),
                map.value(QStringLiteral("c")).toInt()
            };
            return true;
        }
    }

    const QVariantList list = value.toList();
    if (list.size() < 3) {
        return false;
    }

    triangle = {list.at(0).toInt(), list.at(1).toInt(), list.at(2).toInt()};
    return true;
}

QVector<std::array<double, 3>> extractPointArray(const QVariant& value)
{
    QVector<std::array<double, 3>> result;
    const QVariantList list = value.toList();
    result.reserve(list.size());
    for (const QVariant& item : list) {
        std::array<double, 3> point = {0.0, 0.0, 0.0};
        if (extractVec3(item, point)) {
            result.append(point);
        }
    }
    return result;
}

QVector<std::array<int, 3>> extractTriangleArray(const QVariant& value)
{
    QVector<std::array<int, 3>> result;
    const QVariantList list = value.toList();
    result.reserve(list.size());
    for (const QVariant& item : list) {
        std::array<int, 3> triangle = {0, 0, 0};
        if (extractTriangle(item, triangle)) {
            result.append(triangle);
        }
    }
    return result;
}

void configurePlanningModelTargets(ModelNode* modelNode)
{
    if (!modelNode) {
        return;
    }

    DisplayTarget mainTarget;
    mainTarget.visible = true;
    mainTarget.layer = 1;
    modelNode->setWindowDisplayTarget(QStringLiteral("planning_main"), mainTarget);

    DisplayTarget overviewTarget;
    overviewTarget.visible = true;
    overviewTarget.layer = 1;
    modelNode->setWindowDisplayTarget(QStringLiteral("planning_overview"), overviewTarget);
}

void configurePlanningBillboardLineTargets(BillboardLineNode* lineNode)
{
    if (!lineNode) {
        return;
    }

    DisplayTarget mainTarget;
    mainTarget.visible = true;
    mainTarget.layer = 3;
    lineNode->setWindowDisplayTarget(QStringLiteral("planning_main"), mainTarget);

    DisplayTarget overviewTarget;
    overviewTarget.visible = true;
    overviewTarget.layer = 2;
    lineNode->setWindowDisplayTarget(QStringLiteral("planning_overview"), overviewTarget);
}

void configurePlanningArrowTargets(BillboardArrowNode* arrowNode)
{
    if (!arrowNode) {
        return;
    }

    DisplayTarget mainTarget;
    mainTarget.visible = true;
    mainTarget.layer = 3;
    arrowNode->setWindowDisplayTarget(QStringLiteral("planning_main"), mainTarget);

    DisplayTarget overviewTarget;
    overviewTarget.visible = true;
    overviewTarget.layer = 2;
    arrowNode->setWindowDisplayTarget(QStringLiteral("planning_overview"), overviewTarget);
}

void applyPlanPathSegmentStyle(BillboardLineNode* lineNode)
{
    if (!lineNode) {
        return;
    }

    const double lineColor[4] = {0.95, 0.78, 0.12, 1.0};
    lineNode->setColor(lineColor);
    lineNode->setLineWidth(4.0);
    lineNode->setDashed(false);
    lineNode->setDashPattern(0.5, 0.25);
}

void applyPlanArrowStyle(BillboardArrowNode* arrowNode)
{
    if (!arrowNode) {
        return;
    }

    const double arrowColor[4] = {0.95, 0.85, 0.2, 1.0};
    arrowNode->setColor(arrowColor);
    arrowNode->setDirection(QStringLiteral("up"));
    arrowNode->setFollowCameraRotation(false);
    arrowNode->setSize(30.0, 3.0, 14.0, 18.0);
}

bool compareSegmentIndex(BillboardLineNode* lhs, BillboardLineNode* rhs)
{
    if (!lhs || !rhs) {
        return lhs != nullptr;
    }

    const int lhsIndex = lhs->getAttribute(QStringLiteral("segmentIndex"), 0).toInt();
    const int rhsIndex = rhs->getAttribute(QStringLiteral("segmentIndex"), 0).toInt();
    if (lhsIndex != rhsIndex) {
        return lhsIndex < rhsIndex;
    }
    return lhs->getNodeId() < rhs->getNodeId();
}

} // namespace

PlanningModuleLogicHandler::PlanningModuleLogicHandler(QObject* parent)
    : ModuleLogicHandler(QStringLiteral("planning"), parent)
{
}

void PlanningModuleLogicHandler::onModuleActivated()
{
    SceneGraph* scene = getSceneGraph();
    if (!scene) {
        return;
    }

    auto* modelNode = ensurePlanModelNode(scene);
    if (!modelNode) {
        return;
    }

    restorePlanPathVisualizationNodes(scene);
    ensureDefaultPlanGeometry(scene, modelNode);
    if (m_planStatus.isEmpty() || m_planStatus == QStringLiteral("not_started")) {
        m_planStatus = QStringLiteral("ready");
    }

    emitPlanningState(LogicNotification::SceneNodesUpdated);
}

void PlanningModuleLogicHandler::handleAction(const UiAction& action)
{
    if (action.actionType != UiAction::CustomAction) {
        return;
    }

    const QString command = action.payload.value(QStringLiteral("command")).toString().trimmed();

    if (command == PlanningUiCommands::generatePlan()) {
        auto* modelNode = ensurePlanModelNode(getSceneGraph());
        if (!modelNode) {
            return;
        }

        syncPlanPathVisualization(getSceneGraph(), createPlanPath());
        ensureDefaultPlanGeometry(getSceneGraph(), modelNode);
        m_planStatus = QStringLiteral("generated");
        emitPlanningState(LogicNotification::SceneNodesUpdated, action.actionId);
        return;
    }

    if (command == PlanningUiCommands::acceptPlan()) {
        ensurePlanModelNode(getSceneGraph());
        restorePlanPathVisualizationNodes(getSceneGraph());
        m_planStatus = QStringLiteral("accepted");
        emitPlanningState(LogicNotification::StageChanged, action.actionId);
        return;
    }
}

void PlanningModuleLogicHandler::handlePollData(const QString& key, const QVariant& value)
{
    Q_UNUSED(key)
    handleIncomingData(decodeJsonValue(value));
}

void PlanningModuleLogicHandler::handleSubscribeData(const QString& channel,
                                                     const QVariantMap& data)
{
    Q_UNUSED(channel)
    handleIncomingData(data);
}

void PlanningModuleLogicHandler::handleIncomingData(const QVariantMap& payloadData)
{
    SceneGraph* scene = getSceneGraph();
    auto* modelNode = ensurePlanModelNode(scene);
    if (!modelNode) {
        return;
    }

    restorePlanPathVisualizationNodes(scene);

    bool changed = false;

    if (payloadData.contains(QStringLiteral("vertices")) &&
        payloadData.contains(QStringLiteral("triangles"))) {
        const QVector<std::array<double, 3>> vertices = extractPointArray(
            payloadData.value(QStringLiteral("vertices")));
        const QVector<std::array<int, 3>> triangles = extractTriangleArray(
            payloadData.value(QStringLiteral("triangles")));
        if (!vertices.isEmpty() && !triangles.isEmpty()) {
            modelNode->setMeshData(vertices, triangles);
            changed = true;
        }
    }

    if (payloadData.contains(QStringLiteral("path"))) {
        const QVector<std::array<double, 3>> path = extractPointArray(
            payloadData.value(QStringLiteral("path")));
        if (!path.isEmpty()) {
            syncPlanPathVisualization(scene, path);
            changed = true;
        }
    }

    const QString status = payloadData.value(QStringLiteral("status")).toString();
    if (!status.isEmpty()) {
        m_planStatus = status;
        changed = true;
    }

    if (payloadData.value(QStringLiteral("accepted")).toBool()) {
        m_planStatus = QStringLiteral("accepted");
        changed = true;
    }

    if (changed) {
        emitPlanningState(LogicNotification::SceneNodesUpdated);
    }
}

void PlanningModuleLogicHandler::onModuleDeactivated()
{
}

void PlanningModuleLogicHandler::onResync()
{
    restorePlanPathVisualizationNodes(getSceneGraph());
    emitPlanningState(LogicNotification::SceneNodesUpdated);
}

ModelNode* PlanningModuleLogicHandler::ensurePlanModelNode(SceneGraph* scene)
{
    if (!scene) {
        return nullptr;
    }

    if (!m_planModelNodeId.isEmpty()) {
        if (auto* existing = scene->getNodeById<ModelNode>(m_planModelNodeId)) {
            configurePlanningModelTargets(existing);
            return existing;
        }
        m_planModelNodeId.clear();
    }

    if (auto* existing = findPlanModelNode(scene)) {
        m_planModelNodeId = existing->getNodeId();
        configurePlanningModelTargets(existing);
        return existing;
    }

    auto* modelNode = new ModelNode(scene);
    modelNode->setModelRole(QStringLiteral("plan_model"));
    const double modelColor[4] = {0.82, 0.86, 0.91, 0.65};
    modelNode->setColor(modelColor);
    modelNode->setOpacity(0.65);
    modelNode->setShowEdges(true);
    configurePlanningModelTargets(modelNode);
    scene->addNode(modelNode);
    m_planModelNodeId = modelNode->getNodeId();
    return modelNode;
}

BillboardArrowNode* PlanningModuleLogicHandler::ensurePlanPathArrowNode(SceneGraph* scene)
{
    if (!scene) {
        return nullptr;
    }

    if (!m_planPathArrowNodeId.isEmpty()) {
        if (auto* existing = scene->getNodeById<BillboardArrowNode>(m_planPathArrowNodeId)) {
            applyPlanArrowStyle(existing);
            configurePlanningArrowTargets(existing);
            return existing;
        }
        m_planPathArrowNodeId.clear();
    }

    if (auto* existing = findPlanPathArrowNode(scene)) {
        m_planPathArrowNodeId = existing->getNodeId();
        applyPlanArrowStyle(existing);
        configurePlanningArrowTargets(existing);
        return existing;
    }

    auto* arrowNode = new BillboardArrowNode(scene);
    arrowNode->setArrowRole(QString::fromLatin1(kPlanPathArrowRole));
    applyPlanArrowStyle(arrowNode);
    configurePlanningArrowTargets(arrowNode);
    scene->addNode(arrowNode);
    m_planPathArrowNodeId = arrowNode->getNodeId();
    return arrowNode;
}

ModelNode* PlanningModuleLogicHandler::findPlanModelNode(SceneGraph* scene) const
{
    if (!scene) {
        return nullptr;
    }

    const QVector<ModelNode*> nodes = scene->getAllModelNodes();
    for (ModelNode* node : nodes) {
        if (node && node->getModelRole() == QStringLiteral("plan_model")) {
            return node;
        }
    }

    return nullptr;
}

QVector<BillboardLineNode*> PlanningModuleLogicHandler::ensurePlanPathSegmentNodes(SceneGraph* scene,
                                                                                   int segmentCount)
{
    QVector<BillboardLineNode*> nodes;
    if (!scene) {
        return nodes;
    }

    bool allCachedNodesValid = !m_planPathSegmentNodeIds.isEmpty();
    if (allCachedNodesValid) {
        nodes.reserve(m_planPathSegmentNodeIds.size());
        for (const QString& nodeId : m_planPathSegmentNodeIds) {
            auto* node = scene->getNodeById<BillboardLineNode>(nodeId);
            if (!node) {
                allCachedNodesValid = false;
                nodes.clear();
                break;
            }
            nodes.append(node);
        }
    }

    if (!allCachedNodesValid) {
        nodes = findPlanPathSegmentNodes(scene);
    }

    while (nodes.size() > segmentCount) {
        BillboardLineNode* node = nodes.takeLast();
        if (node) {
            scene->removeNode(node->getNodeId());
        }
    }

    while (nodes.size() < segmentCount) {
        auto* lineNode = new BillboardLineNode(scene);
        lineNode->setLineRole(QString::fromLatin1(kPlanPathSegmentRole));
        lineNode->setAttribute(QStringLiteral("segmentIndex"), nodes.size());
        applyPlanPathSegmentStyle(lineNode);
        configurePlanningBillboardLineTargets(lineNode);
        scene->addNode(lineNode);
        nodes.append(lineNode);
    }

    m_planPathSegmentNodeIds.clear();
    for (int index = 0; index < nodes.size(); ++index) {
        BillboardLineNode* node = nodes.at(index);
        if (!node) {
            continue;
        }

        node->setAttribute(QStringLiteral("segmentIndex"), index);
        node->setLineRole(QString::fromLatin1(kPlanPathSegmentRole));
        applyPlanPathSegmentStyle(node);
        configurePlanningBillboardLineTargets(node);
        m_planPathSegmentNodeIds.append(node->getNodeId());
    }

    return nodes;
}

BillboardArrowNode* PlanningModuleLogicHandler::findPlanPathArrowNode(SceneGraph* scene) const
{
    if (!scene) {
        return nullptr;
    }

    const QVector<BillboardArrowNode*> nodes = scene->getAllBillboardArrowNodes();
    for (BillboardArrowNode* node : nodes) {
        if (node && node->getArrowRole() == QString::fromLatin1(kPlanPathArrowRole)) {
            return node;
        }
    }

    return nullptr;
}

QVector<BillboardLineNode*> PlanningModuleLogicHandler::findPlanPathSegmentNodes(SceneGraph* scene) const
{
    QVector<BillboardLineNode*> result;
    if (!scene) {
        return result;
    }

    const QVector<BillboardLineNode*> nodes = scene->getAllBillboardLineNodes();
    result.reserve(nodes.size());
    for (BillboardLineNode* node : nodes) {
        if (node && node->getLineRole() == QString::fromLatin1(kPlanPathSegmentRole)) {
            result.append(node);
        }
    }

    std::sort(result.begin(), result.end(), compareSegmentIndex);
    return result;
}

void PlanningModuleLogicHandler::restorePlanPathVisualizationNodes(SceneGraph* scene)
{
    if (!scene) {
        return;
    }

    removeLegacyPlanPathNodes(scene);

    const QVector<BillboardLineNode*> segments = findPlanPathSegmentNodes(scene);
    m_planPathSegmentNodeIds.clear();
    for (int index = 0; index < segments.size(); ++index) {
        BillboardLineNode* node = segments.at(index);
        if (!node) {
            continue;
        }
        node->setAttribute(QStringLiteral("segmentIndex"), index);
        node->setLineRole(QString::fromLatin1(kPlanPathSegmentRole));
        applyPlanPathSegmentStyle(node);
        configurePlanningBillboardLineTargets(node);
        m_planPathSegmentNodeIds.append(node->getNodeId());
    }

    if (segments.isEmpty()) {
        removePlanPathArrowNode(scene);
        return;
    }

    BillboardArrowNode* arrowNode = ensurePlanPathArrowNode(scene);
    if (!arrowNode) {
        return;
    }

    arrowNode->setTipPoint(segments.last()->getEndPoint());
}

void PlanningModuleLogicHandler::syncPlanPathVisualization(
    SceneGraph* scene,
    const QVector<std::array<double, 3>>& pathPoints)
{
    if (!scene) {
        return;
    }

    removeLegacyPlanPathNodes(scene);

    const int pointCount = static_cast<int>(pathPoints.size());
    const int segmentCount = pointCount > 1 ? (pointCount - 1) : 0;
    QVector<BillboardLineNode*> segments = ensurePlanPathSegmentNodes(scene, segmentCount);

    if (segmentCount <= 0) {
        removePlanPathArrowNode(scene);
        return;
    }

    for (int index = 0; index < segmentCount; ++index) {
        BillboardLineNode* segmentNode = segments.value(index, nullptr);
        if (!segmentNode) {
            continue;
        }
        segmentNode->setEndpoints(pathPoints.at(index), pathPoints.at(index + 1));
    }

    if (BillboardArrowNode* arrowNode = ensurePlanPathArrowNode(scene)) {
        arrowNode->setTipPoint(pathPoints.last());
    }
}

void PlanningModuleLogicHandler::removeLegacyPlanPathNodes(SceneGraph* scene) const
{
    if (!scene) {
        return;
    }

    const QVector<LineNode*> legacyNodes = scene->getAllLineNodes();
    for (LineNode* node : legacyNodes) {
        if (node && node->getLineRole() == QStringLiteral("plan_path")) {
            scene->removeNode(node->getNodeId());
        }
    }
}

void PlanningModuleLogicHandler::removePlanPathArrowNode(SceneGraph* scene)
{
    if (!scene) {
        return;
    }

    if (!m_planPathArrowNodeId.isEmpty()) {
        scene->removeNode(m_planPathArrowNodeId);
        m_planPathArrowNodeId.clear();
        return;
    }

    if (BillboardArrowNode* arrowNode = findPlanPathArrowNode(scene)) {
        scene->removeNode(arrowNode->getNodeId());
    }
    m_planPathArrowNodeId.clear();
}

void PlanningModuleLogicHandler::ensureDefaultPlanGeometry(SceneGraph* scene, ModelNode* modelNode)
{
    if (modelNode && !modelNode->getPolyData()) {
        modelNode->setMeshData(createPlanVertices(), createPlanTriangles());
    }

    if (scene && m_planPathSegmentNodeIds.isEmpty()) {
        syncPlanPathVisualization(scene, createPlanPath());
    }
}

void PlanningModuleLogicHandler::emitPlanningState(LogicNotification::EventType eventType,
                                                   const QString& sourceActionId,
                                                   const QString& sourceSampleId)
{
    QVariantMap payload;
    payload.insert(QStringLiteral("planModelNodeId"), m_planModelNodeId);
    payload.insert(QStringLiteral("planPathNodeId"),
                   m_planPathSegmentNodeIds.isEmpty() ? QString() : m_planPathSegmentNodeIds.first());
    payload.insert(QStringLiteral("planPathSegmentNodeIds"), m_planPathSegmentNodeIds);
    payload.insert(QStringLiteral("planPathArrowNodeId"), m_planPathArrowNodeId);
    payload.insert(QStringLiteral("status"), m_planStatus);
    if (!sourceSampleId.isEmpty()) {
        payload.insert(QStringLiteral("sourceSampleId"), sourceSampleId);
    }

    LogicNotification notification = LogicNotification::create(
        eventType,
        LogicNotification::CurrentModule,
        payload);
    notification.setSourceActionId(sourceActionId);
    emit logicNotification(notification);
}
