#include "PlanningModuleLogicHandler.h"

#include "logic/scene/SceneGraph.h"
#include "logic/scene/nodes/LineNode.h"
#include "logic/scene/nodes/ModelNode.h"

#include <array>

namespace {

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

QVariantMap normalizedSampleData(const StateSample& sample)
{
    QVariantMap payload = sample.data.value(QStringLiteral("value")).toMap();
    return payload.isEmpty() ? sample.data : payload;
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

void configurePlanningPathTargets(LineNode* lineNode)
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
    auto* lineNode = ensurePlanPathNode(scene);
    if (!modelNode || !lineNode) {
        return;
    }

    ensureDefaultPlanGeometry(modelNode, lineNode);
    if (m_planStatus.isEmpty() || m_planStatus == QStringLiteral("not_started")) {
        m_planStatus = QStringLiteral("ready");
    }

    emitPlanningState(LogicNotification::SceneNodesUpdated);
}

void PlanningModuleLogicHandler::handleAction(const UiAction& action)
{
    if (action.actionType == UiAction::GeneratePlan) {
        auto* modelNode = ensurePlanModelNode(getSceneGraph());
        auto* lineNode = ensurePlanPathNode(getSceneGraph());
        if (!modelNode || !lineNode) {
            return;
        }

        ensureDefaultPlanGeometry(modelNode, lineNode);
        m_planStatus = QStringLiteral("generated");
        emitPlanningState(LogicNotification::SceneNodesUpdated, action.actionId);
    } else if (action.actionType == UiAction::AcceptPlan) {
        ensurePlanModelNode(getSceneGraph());
        ensurePlanPathNode(getSceneGraph());
        m_planStatus = QStringLiteral("accepted");
        emitPlanningState(LogicNotification::StageChanged, action.actionId);
        return;
    }

    if (action.actionType == UiAction::CustomAction) {
        Q_UNUSED(action);
    }
}

void PlanningModuleLogicHandler::handleStateSample(const StateSample& sample)
{
    SceneGraph* scene = getSceneGraph();
    auto* modelNode = ensurePlanModelNode(scene);
    auto* lineNode = ensurePlanPathNode(scene);
    if (!modelNode || !lineNode) {
        return;
    }

    const QVariantMap payloadData = normalizedSampleData(sample);
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
            lineNode->setPolyline(path);
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
        emitPlanningState(LogicNotification::SceneNodesUpdated, QString(), sample.sampleId);
    }
}

void PlanningModuleLogicHandler::onModuleDeactivated()
{
}

void PlanningModuleLogicHandler::onResync()
{
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

LineNode* PlanningModuleLogicHandler::ensurePlanPathNode(SceneGraph* scene)
{
    if (!scene) {
        return nullptr;
    }

    if (!m_planPathNodeId.isEmpty()) {
        if (auto* existing = scene->getNodeById<LineNode>(m_planPathNodeId)) {
            configurePlanningPathTargets(existing);
            return existing;
        }
        m_planPathNodeId.clear();
    }

    if (auto* existing = findPlanPathNode(scene)) {
        m_planPathNodeId = existing->getNodeId();
        configurePlanningPathTargets(existing);
        return existing;
    }

    auto* lineNode = new LineNode(scene);
    lineNode->setLineRole(QStringLiteral("plan_path"));
    const double lineColor[4] = {0.95, 0.78, 0.12, 1.0};
    lineNode->setColor(lineColor);
    lineNode->setLineWidth(4.0);
    configurePlanningPathTargets(lineNode);
    scene->addNode(lineNode);
    m_planPathNodeId = lineNode->getNodeId();
    return lineNode;
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

LineNode* PlanningModuleLogicHandler::findPlanPathNode(SceneGraph* scene) const
{
    if (!scene) {
        return nullptr;
    }

    const QVector<LineNode*> nodes = scene->getAllLineNodes();
    for (LineNode* node : nodes) {
        if (node && node->getLineRole() == QStringLiteral("plan_path")) {
            return node;
        }
    }

    return nullptr;
}

void PlanningModuleLogicHandler::ensureDefaultPlanGeometry(ModelNode* modelNode, LineNode* pathNode) const
{
    if (modelNode && !modelNode->getPolyData()) {
        modelNode->setMeshData(createPlanVertices(), createPlanTriangles());
    }

    if (pathNode && pathNode->getVertexCount() == 0) {
        pathNode->setPolyline(createPlanPath());
    }
}

void PlanningModuleLogicHandler::emitPlanningState(LogicNotification::EventType eventType,
                                                   const QString& sourceActionId,
                                                   const QString& sourceSampleId)
{
    QVariantMap payload;
    payload.insert(QStringLiteral("planModelNodeId"), m_planModelNodeId);
    payload.insert(QStringLiteral("planPathNodeId"), m_planPathNodeId);
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
