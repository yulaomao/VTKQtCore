#include "DataGenModuleLogicHandler.h"

#include "logic/scene/SceneGraph.h"
#include "logic/scene/nodes/LineNode.h"
#include "logic/scene/nodes/ModelNode.h"
#include "logic/scene/nodes/NodeBase.h"
#include "logic/scene/nodes/PointNode.h"
#include "logic/scene/nodes/TransformNode.h"

#include <vtkCubeSource.h>
#include <vtkCylinderSource.h>
#include <vtkConeSource.h>
#include <vtkPolyData.h>
#include <vtkSphereSource.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>
#include <vtkMatrix4x4.h>

#include <QDateTime>
#include <QUuid>

#include <QtMath>

namespace {

QString dataGenStateRedisKey()
{
    return QStringLiteral("state.datagen.latest");
}

QString dataGenStateRedisChannel()
{
    return QStringLiteral("state.datagen");
}

QString persistIdAttributeName()
{
    return QStringLiteral("datagenPersistId");
}

QString typeKeyForNode(const NodeBase* node)
{
    if (dynamic_cast<const PointNode*>(node)) {
        return QStringLiteral("point");
    }
    if (dynamic_cast<const LineNode*>(node)) {
        return QStringLiteral("line");
    }
    if (dynamic_cast<const ModelNode*>(node)) {
        return QStringLiteral("model");
    }
    if (dynamic_cast<const TransformNode*>(node)) {
        return QStringLiteral("transform");
    }
    return QStringLiteral("node");
}

QString nodeNameOrFallback(const NodeBase* node)
{
    if (!node) {
        return QString();
    }
    return node->getName().isEmpty() ? node->getNodeId() : node->getName();
}

QString parentTransformId(const NodeBase* node)
{
    return node ? node->getFirstReference(NodeBase::parentTransformReferenceRole()) : QString();
}

void configureColor(double out[4], double r, double g, double b, double a)
{
    out[0] = r;
    out[1] = g;
    out[2] = b;
    out[3] = a;
}

vtkSmartPointer<vtkPolyData> buildShapePolyData(const QString& shape,
                                                double sizeA,
                                                double sizeB,
                                                double sizeC,
                                                int resolution)
{
    const QString normalized = shape.trimmed().toLower();

    if (normalized == QStringLiteral("cube")) {
        auto source = vtkSmartPointer<vtkCubeSource>::New();
        source->SetXLength(sizeA);
        source->SetYLength(sizeB);
        source->SetZLength(sizeC);
        source->Update();
        return source->GetOutput();
    }

    if (normalized == QStringLiteral("cylinder")) {
        auto source = vtkSmartPointer<vtkCylinderSource>::New();
        source->SetRadius(sizeA * 0.5);
        source->SetHeight(sizeC);
        source->SetResolution(qMax(6, resolution));
        source->CappingOn();
        source->Update();
        return source->GetOutput();
    }

    if (normalized == QStringLiteral("cone")) {
        auto source = vtkSmartPointer<vtkConeSource>::New();
        source->SetRadius(sizeA * 0.5);
        source->SetHeight(sizeC);
        source->SetResolution(qMax(6, resolution));
        source->SetDirection(0.0, 0.0, 1.0);
        source->Update();
        return source->GetOutput();
    }

    auto source = vtkSmartPointer<vtkSphereSource>::New();
    source->SetRadius(sizeA * 0.5);
    source->SetThetaResolution(qMax(8, resolution));
    source->SetPhiResolution(qMax(8, resolution));
    source->Update();
    return source->GetOutput();
}

void buildPoseMatrix(double tx,
                     double ty,
                     double tz,
                     double rx,
                     double ry,
                     double rz,
                     double out[16])
{
    auto transform = vtkSmartPointer<vtkTransform>::New();
    transform->PostMultiply();
    transform->Identity();
    transform->Translate(tx, ty, tz);
    transform->RotateZ(rz);
    transform->RotateY(ry);
    transform->RotateX(rx);

    vtkMatrix4x4* matrix = transform->GetMatrix();
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            out[column * 4 + row] = matrix->GetElement(row, column);
        }
    }
}

double attributeAsDouble(const NodeBase* node, const QString& key)
{
    return node ? node->getAttribute(key, 0.0).toDouble() : 0.0;
}

QString persistIdForNode(const NodeBase* node)
{
    return node ? node->getAttribute(persistIdAttributeName()).toString() : QString();
}

QVariantList toPointVariantList(const QVector<std::array<double, 3>>& points)
{
    QVariantList result;
    result.reserve(points.size());
    for (const auto& point : points) {
        result.append(QVariantList{point[0], point[1], point[2]});
    }
    return result;
}

QVariantList toTriangleVariantList(const QVector<std::array<int, 3>>& triangles)
{
    QVariantList result;
    result.reserve(triangles.size());
    for (const auto& triangle : triangles) {
        result.append(QVariantList{triangle[0], triangle[1], triangle[2]});
    }
    return result;
}

QVariantList toMatrixVariantList(const double matrix[16])
{
    QVariantList result;
    result.reserve(16);
    for (int index = 0; index < 16; ++index) {
        result.append(matrix[index]);
    }
    return result;
}

QVector<std::array<double, 3>> fromPointVariantList(const QVariantList& points)
{
    QVector<std::array<double, 3>> result;
    result.reserve(points.size());
    for (const QVariant& item : points) {
        const QVariantList point = item.toList();
        if (point.size() < 3) {
            continue;
        }
        result.push_back({
            point.at(0).toDouble(),
            point.at(1).toDouble(),
            point.at(2).toDouble()
        });
    }
    return result;
}

QVector<std::array<int, 3>> fromTriangleVariantList(const QVariantList& triangles)
{
    QVector<std::array<int, 3>> result;
    result.reserve(triangles.size());
    for (const QVariant& item : triangles) {
        const QVariantList triangle = item.toList();
        if (triangle.size() < 3) {
            continue;
        }
        result.push_back({
            triangle.at(0).toInt(),
            triangle.at(1).toInt(),
            triangle.at(2).toInt()
        });
    }
    return result;
}

bool fillMatrixFromVariantList(const QVariantList& values, double out[16])
{
    if (values.size() != 16) {
        return false;
    }

    for (int index = 0; index < 16; ++index) {
        out[index] = values.at(index).toDouble();
    }
    return true;
}

}

DataGenModuleLogicHandler::DataGenModuleLogicHandler(QObject* parent)
    : ModuleLogicHandler(QStringLiteral("datagen"), parent)
{
}

QString DataGenModuleLogicHandler::moduleOwnerTag()
{
    return QStringLiteral("datagen");
}

void DataGenModuleLogicHandler::handleAction(const UiAction& action)
{
    if (action.actionType != UiAction::CustomAction) {
        return;
    }

    handleCustomCommand(action.payload, action.actionId);
}

ModuleInvokeResult DataGenModuleLogicHandler::handleModuleInvoke(const ModuleInvokeRequest& request)
{
    QString command = request.method.trimmed();
    if (command.isEmpty()) {
        command = request.payload.value(QStringLiteral("command")).toString().trimmed();
    }

    if (command.isEmpty()) {
        return ModuleInvokeResult::failure(
            QStringLiteral("datagen_command_missing"),
            QStringLiteral("Datagen invoke request is missing command/method"),
            {{QStringLiteral("sourceModule"), request.sourceModule}});
    }

    if (command == QStringLiteral("seed_demo")) {
        ensureSeedScene();
        persistRedisSnapshot(QStringLiteral("seed_demo"));
        emitState(QStringLiteral("演示层级已准备完毕。"));
        return ModuleInvokeResult::success(
            {{QStringLiteral("selectedNodeId"), m_selectedNodeId}},
            QStringLiteral("Datagen demo scene created"));
    }

    if (command == QStringLiteral("create_node")) {
        const QString nodeType = request.payload.value(QStringLiteral("nodeType")).toString();
        NodeBase* created = nullptr;
        if (nodeType == QStringLiteral("point")) {
            created = createPointNode(request.payload);
        } else if (nodeType == QStringLiteral("line")) {
            created = createLineNode(request.payload);
        } else if (nodeType == QStringLiteral("model")) {
            created = createModelNode(request.payload);
        } else if (nodeType == QStringLiteral("transform")) {
            created = createTransformNode(request.payload);
        }

        if (!created) {
            return ModuleInvokeResult::failure(
                QStringLiteral("datagen_node_create_failed"),
                QStringLiteral("Datagen could not create node for type '%1'").arg(nodeType),
                {{QStringLiteral("nodeType"), nodeType}});
        }

        m_selectedNodeId = created->getNodeId();
        persistRedisSnapshot(
            QStringLiteral("node_created"),
            persistIdForNode(created),
            nodeNameOrFallback(created));
        emitState(QStringLiteral("已创建 %1。").arg(nodeNameOrFallback(created)));
        return ModuleInvokeResult::success(
            {{QStringLiteral("nodeId"), created->getNodeId()},
             {QStringLiteral("persistId"), persistIdForNode(created)},
             {QStringLiteral("name"), nodeNameOrFallback(created)},
             {QStringLiteral("nodeType"), nodeType}},
            QStringLiteral("Datagen node created"));
    }

    return ModuleInvokeResult::failure(
        QStringLiteral("datagen_invoke_unsupported"),
        QStringLiteral("Datagen does not support invoke method '%1'").arg(command),
        {{QStringLiteral("sourceModule"), request.sourceModule}});
}

void DataGenModuleLogicHandler::onModuleActivated()
{
    bool restored = false;
    if (managedNodes().isEmpty() && hasRedisCommandAccess()) {
        restored = restoreFromRedisSnapshot(readRedisJsonValue(dataGenStateRedisKey()));
    }

    if (!restored) {
        const bool hadNodes = !managedNodes().isEmpty();
        ensureSeedScene();
        if (!hadNodes && !managedNodes().isEmpty()) {
            persistRedisSnapshot(QStringLiteral("seed_default_scene"));
        }
    }

    emitState(m_statusText);
}

void DataGenModuleLogicHandler::onResync()
{
    if (hasRedisCommandAccess()) {
        const QVariantMap snapshot = readRedisJsonValue(dataGenStateRedisKey());
        if (restoreFromRedisSnapshot(snapshot)) {
            emitState(QStringLiteral("DataGen 模块已从 Redis 重同步。"));
            return;
        }
    }

    emitState(QStringLiteral("DataGen 模块已重同步。"));
}

void DataGenModuleLogicHandler::ensureSeedScene()
{
    if (!managedNodes().isEmpty()) {
        selectFallbackNode();
        return;
    }

    auto* rootTransform = createTransformNode({
        {QStringLiteral("name"), QStringLiteral("Generator Root")},
        {QStringLiteral("showAxes"), true},
        {QStringLiteral("axesLength"), 90.0}
    });
    if (!rootTransform) {
        return;
    }
    updateTransformPose(rootTransform, {
        {QStringLiteral("tx"), 0.0},
        {QStringLiteral("ty"), 0.0},
        {QStringLiteral("tz"), 0.0},
        {QStringLiteral("rx"), 0.0},
        {QStringLiteral("ry"), 0.0},
        {QStringLiteral("rz"), 0.0}
    });

    auto* pointNode = createPointNode({
        {QStringLiteral("name"), QStringLiteral("Fiducial Cloud")},
        {QStringLiteral("count"), 6},
        {QStringLiteral("spacing"), 14.0}
    });
    auto* lineNode = createLineNode({
        {QStringLiteral("name"), QStringLiteral("Guide Path")},
        {QStringLiteral("count"), 5},
        {QStringLiteral("spacing"), 28.0},
        {QStringLiteral("closed"), false}
    });
    auto* modelNode = createModelNode({
        {QStringLiteral("name"), QStringLiteral("Target Sphere")},
        {QStringLiteral("shape"), QStringLiteral("sphere")},
        {QStringLiteral("sizeA"), 42.0},
        {QStringLiteral("sizeB"), 42.0},
        {QStringLiteral("sizeC"), 42.0},
        {QStringLiteral("resolution"), 28}
    });
    auto* childTransform = createTransformNode({
        {QStringLiteral("name"), QStringLiteral("Tool Frame")},
        {QStringLiteral("showAxes"), true},
        {QStringLiteral("axesLength"), 64.0}
    });

    if (childTransform) {
        assignParent(childTransform, rootTransform->getNodeId());
        updateTransformPose(childTransform, {
            {QStringLiteral("tx"), 48.0},
            {QStringLiteral("ty"), 18.0},
            {QStringLiteral("tz"), 24.0},
            {QStringLiteral("rx"), 0.0},
            {QStringLiteral("ry"), 25.0},
            {QStringLiteral("rz"), -18.0}
        });
    }

    if (pointNode) {
        assignParent(pointNode, rootTransform->getNodeId());
    }
    if (lineNode) {
        assignParent(lineNode, rootTransform->getNodeId());
    }
    if (modelNode && childTransform) {
        assignParent(modelNode, childTransform->getNodeId());
    }

    m_selectedNodeId = rootTransform->getNodeId();
    m_statusText = QStringLiteral("已生成默认演示层级，可直接继续编辑。");
}

void DataGenModuleLogicHandler::handleCustomCommand(const QVariantMap& payload, const QString& sourceActionId)
{
    const QString command = payload.value(QStringLiteral("command")).toString();
    if (command.isEmpty()) {
        return;
    }

    if (command == QStringLiteral("seed_demo")) {
        ensureSeedScene();
        persistRedisSnapshot(QStringLiteral("seed_demo"));
        emitState(QStringLiteral("演示层级已准备完毕。"), LogicNotification::SceneNodesUpdated, sourceActionId);
        return;
    }

    if (command == QStringLiteral("select_node")) {
        m_selectedNodeId = payload.value(QStringLiteral("nodeId")).toString();
        emitState(QStringLiteral("已选择节点。"), LogicNotification::SceneNodesUpdated, sourceActionId);
        return;
    }

    if (command == QStringLiteral("create_node")) {
        NodeBase* created = nullptr;
        const QString nodeType = payload.value(QStringLiteral("nodeType")).toString();
        if (nodeType == QStringLiteral("point")) {
            created = createPointNode(payload);
        } else if (nodeType == QStringLiteral("line")) {
            created = createLineNode(payload);
        } else if (nodeType == QStringLiteral("model")) {
            created = createModelNode(payload);
        } else if (nodeType == QStringLiteral("transform")) {
            created = createTransformNode(payload);
        }

        if (created) {
            m_selectedNodeId = created->getNodeId();
            persistRedisSnapshot(
                QStringLiteral("node_created"),
                persistIdForNode(created),
                nodeNameOrFallback(created));
            emitState(QStringLiteral("已创建 %1。")
                          .arg(nodeNameOrFallback(created)),
                      LogicNotification::SceneNodesUpdated,
                      sourceActionId);
        }
        return;
    }

    NodeBase* node = nodeById(payload.value(QStringLiteral("nodeId")).toString());
    if (!node) {
        emitState(QStringLiteral("目标节点不存在或不属于 datagen。"),
                  LogicNotification::SceneNodesUpdated,
                  sourceActionId);
        return;
    }

    if (command == QStringLiteral("delete_node")) {
        const QString deletedName = nodeNameOrFallback(node);
        const QString deletedPersistId = persistIdForNode(node);
        if (deleteNode(node->getNodeId())) {
            persistRedisSnapshot(QStringLiteral("node_deleted"), deletedPersistId, deletedName);
            emitState(QStringLiteral("已删除 %1。").arg(deletedName),
                      LogicNotification::SceneNodesUpdated,
                      sourceActionId);
        }
        return;
    }

    if (command == QStringLiteral("clear_node_geometry")) {
        clearNodeGeometry(node);
        persistRedisSnapshot(
            QStringLiteral("node_geometry_cleared"),
            persistIdForNode(node),
            nodeNameOrFallback(node));
        emitState(QStringLiteral("已清空节点数据。"),
                  LogicNotification::SceneNodesUpdated,
                  sourceActionId);
        return;
    }

    if (command == QStringLiteral("update_display")) {
        updateDisplay(node, payload);
        persistRedisSnapshot(
            QStringLiteral("node_display_updated"),
            persistIdForNode(node),
            nodeNameOrFallback(node));
        emitState(QStringLiteral("显示属性已更新。"),
                  LogicNotification::SceneNodesUpdated,
                  sourceActionId);
        return;
    }

    if (command == QStringLiteral("assign_parent")) {
        assignParent(node, payload.value(QStringLiteral("parentTransformId")).toString());
        persistRedisSnapshot(
            QStringLiteral("node_parent_updated"),
            persistIdForNode(node),
            nodeNameOrFallback(node));
        emitState(QStringLiteral("父变换关系已更新。"),
                  LogicNotification::SceneNodesUpdated,
                  sourceActionId);
        return;
    }

    if (command == QStringLiteral("update_transform_pose")) {
        if (auto* transformNode = dynamic_cast<TransformNode*>(node)) {
            updateTransformPose(transformNode, payload);
            persistRedisSnapshot(
                QStringLiteral("transform_pose_updated"),
                persistIdForNode(transformNode),
                nodeNameOrFallback(transformNode));
            emitState(QStringLiteral("局部变换已更新。"),
                      LogicNotification::SceneNodesUpdated,
                      sourceActionId);
        }
        return;
    }

    if (command == QStringLiteral("add_point")) {
        if (auto* pointNode = dynamic_cast<PointNode*>(node)) {
            PointItem item;
            item.label = payload.value(QStringLiteral("label")).toString();
            item.position[0] = payload.value(QStringLiteral("x")).toDouble();
            item.position[1] = payload.value(QStringLiteral("y")).toDouble();
            item.position[2] = payload.value(QStringLiteral("z")).toDouble();
            pointNode->addPoint(item);
            persistRedisSnapshot(
                QStringLiteral("point_added"),
                persistIdForNode(pointNode),
                nodeNameOrFallback(pointNode));
            emitState(QStringLiteral("已向 PointNode 添加点。"),
                      LogicNotification::SceneNodesUpdated,
                      sourceActionId);
        }
        return;
    }

    if (command == QStringLiteral("add_line_vertex")) {
        if (auto* lineNode = dynamic_cast<LineNode*>(node)) {
            lineNode->appendVertex({
                payload.value(QStringLiteral("x")).toDouble(),
                payload.value(QStringLiteral("y")).toDouble(),
                payload.value(QStringLiteral("z")).toDouble()
            });
            persistRedisSnapshot(
                QStringLiteral("line_vertex_added"),
                persistIdForNode(lineNode),
                nodeNameOrFallback(lineNode));
            emitState(QStringLiteral("已向 LineNode 添加顶点。"),
                      LogicNotification::SceneNodesUpdated,
                      sourceActionId);
        }
        return;
    }
}

void DataGenModuleLogicHandler::emitState(const QString& statusText,
                                          LogicNotification::EventType eventType,
                                          const QString& sourceActionId)
{
    if (!statusText.isEmpty()) {
        m_statusText = statusText;
    }

    LogicNotification notification = LogicNotification::create(
        eventType,
        LogicNotification::CurrentModule,
        buildState(m_statusText));
    notification.setSourceActionId(sourceActionId);
    emit logicNotification(notification);
}

QVariantMap DataGenModuleLogicHandler::buildState(const QString& statusText) const
{
    NodeBase* selectedNode = nodeById(m_selectedNodeId);
    if (!selectedNode) {
        const QVector<NodeBase*> nodes = managedNodes();
        selectedNode = nodes.isEmpty() ? nullptr : nodes.first();
    }

    return {
        {QStringLiteral("statusText"), statusText},
        {QStringLiteral("nodeSummaries"), buildNodeSummaries()},
        {QStringLiteral("transformOptions"), buildTransformOptions()},
        {QStringLiteral("selectedNodeId"), selectedNode ? selectedNode->getNodeId() : QString()},
        {QStringLiteral("selectedParentTransformId"), parentTransformId(selectedNode)},
        {QStringLiteral("selectedNodeDetails"), buildNodeDetails(selectedNode)}
    };
}

QVariantMap DataGenModuleLogicHandler::buildRedisSnapshot(const QString& changeEvent,
                                                          const QString& changedNodePersistId,
                                                          const QString& changedNodeName) const
{
    QVariantList serializedNodes;
    const QVector<NodeBase*> nodes = managedNodes();
    serializedNodes.reserve(nodes.size());
    for (NodeBase* node : nodes) {
        serializedNodes.append(serializeNodeForRedis(node));
    }

    NodeBase* selectedNode = nodeById(m_selectedNodeId);
    QVariantMap snapshot = buildState(m_statusText);
    snapshot.insert(QStringLiteral("eventName"), changeEvent);
    snapshot.insert(QStringLiteral("changedNodePersistId"), changedNodePersistId);
    snapshot.insert(QStringLiteral("changedNodeName"), changedNodeName);
    snapshot.insert(QStringLiteral("selectedNodePersistId"), persistIdForNode(selectedNode));
    snapshot.insert(QStringLiteral("nodes"), serializedNodes);
    snapshot.insert(QStringLiteral("nodeCount"), serializedNodes.size());
    snapshot.insert(QStringLiteral("timestampMs"), QDateTime::currentMSecsSinceEpoch());
    return snapshot;
}

QVariantList DataGenModuleLogicHandler::buildNodeSummaries() const
{
    QVariantList result;
    for (NodeBase* node : managedNodes()) {
        const QString parentId = parentTransformId(node);
        NodeBase* parentNode = nodeById(parentId);
        result.append(QVariantMap{
            {QStringLiteral("id"), node->getNodeId()},
            {QStringLiteral("name"), nodeNameOrFallback(node)},
            {QStringLiteral("type"), typeKeyForNode(node)},
            {QStringLiteral("parentId"), parentId},
            {QStringLiteral("parentName"), nodeNameOrFallback(parentNode)}
        });
    }
    return result;
}

QVariantList DataGenModuleLogicHandler::buildTransformOptions() const
{
    QVariantList result;
    for (TransformNode* node : managedTransforms()) {
        result.append(QVariantMap{
            {QStringLiteral("id"), node->getNodeId()},
            {QStringLiteral("name"), nodeNameOrFallback(node)}
        });
    }
    return result;
}

QVariantMap DataGenModuleLogicHandler::buildNodeDetails(NodeBase* node) const
{
    if (!node) {
        return {};
    }

    DisplayTarget target = node->getDisplayTargetForWindow(QStringLiteral("datagen_main"));
    QVariantMap details{
        {QStringLiteral("id"), node->getNodeId()},
        {QStringLiteral("name"), nodeNameOrFallback(node)},
        {QStringLiteral("type"), typeKeyForNode(node)},
        {QStringLiteral("visible"), target.visible},
        {QStringLiteral("layer"), target.layer},
        {QStringLiteral("parentName"), nodeNameOrFallback(nodeById(parentTransformId(node)))},
        {QStringLiteral("parentId"), parentTransformId(node)},
        {QStringLiteral("tx"), attributeAsDouble(node, QStringLiteral("poseTx"))},
        {QStringLiteral("ty"), attributeAsDouble(node, QStringLiteral("poseTy"))},
        {QStringLiteral("tz"), attributeAsDouble(node, QStringLiteral("poseTz"))},
        {QStringLiteral("rx"), attributeAsDouble(node, QStringLiteral("poseRx"))},
        {QStringLiteral("ry"), attributeAsDouble(node, QStringLiteral("poseRy"))},
        {QStringLiteral("rz"), attributeAsDouble(node, QStringLiteral("poseRz"))}
    };

    double color[4] = {1.0, 1.0, 1.0, 1.0};
    if (auto* pointNode = dynamic_cast<PointNode*>(node)) {
        pointNode->getDefaultPointColor(color);
        details.insert(QStringLiteral("opacity"), color[3]);
        details.insert(QStringLiteral("sizeValue"), pointNode->getDefaultPointSize());
        details.insert(QStringLiteral("showLabels"), pointNode->isShowPointLabel());
        details.insert(QStringLiteral("pointCount"), pointNode->getPointCount());
    } else if (auto* lineNode = dynamic_cast<LineNode*>(node)) {
        lineNode->getColor(color);
        details.insert(QStringLiteral("opacity"), lineNode->getOpacity());
        details.insert(QStringLiteral("sizeValue"), lineNode->getLineWidth());
        details.insert(QStringLiteral("renderMode"), lineNode->getRenderMode());
        details.insert(QStringLiteral("dashed"), lineNode->isDashed());
        details.insert(QStringLiteral("vertexCount"), lineNode->getVertexCount());
        details.insert(QStringLiteral("length"), lineNode->getLength());
    } else if (auto* modelNode = dynamic_cast<ModelNode*>(node)) {
        modelNode->getColor(color);
        details.insert(QStringLiteral("opacity"), modelNode->getOpacity());
        details.insert(QStringLiteral("renderMode"), modelNode->getRenderMode());
        details.insert(QStringLiteral("showEdges"), modelNode->isShowEdges());
        details.insert(QStringLiteral("triangleCount"), modelNode->getIndices().size());
        details.insert(QStringLiteral("shape"), modelNode->getAttribute(
            QStringLiteral("geometryPreset"), QStringLiteral("mesh")).toString());
    } else if (auto* transformNode = dynamic_cast<TransformNode*>(node)) {
        transformNode->getAxesColorX(color);
        details.insert(QStringLiteral("opacity"), color[3]);
        details.insert(QStringLiteral("showAxes"), transformNode->isShowAxes());
        details.insert(QStringLiteral("sizeValue"), transformNode->getAxesLength());
        details.insert(QStringLiteral("sourceSpace"), transformNode->getSourceSpace());
        details.insert(QStringLiteral("targetSpace"), transformNode->getTargetSpace());
    }

    details.insert(QStringLiteral("red"), color[0]);
    details.insert(QStringLiteral("green"), color[1]);
    details.insert(QStringLiteral("blue"), color[2]);
    return details;
}

QVariantMap DataGenModuleLogicHandler::serializeNodeForRedis(NodeBase* node) const
{
    if (!node) {
        return {};
    }

    const DisplayTarget target = node->getDisplayTargetForWindow(QStringLiteral("datagen_main"));
    QVariantMap payload{
        {QStringLiteral("persistId"), persistIdForNode(node)},
        {QStringLiteral("name"), nodeNameOrFallback(node)},
        {QStringLiteral("type"), typeKeyForNode(node)},
        {QStringLiteral("parentPersistId"), persistIdForNode(nodeById(parentTransformId(node)))},
        {QStringLiteral("display"), QVariantMap{
            {QStringLiteral("visible"), target.visible},
            {QStringLiteral("layer"), target.layer}
        }}
    };

    if (auto* pointNode = dynamic_cast<PointNode*>(node)) {
        double defaultColor[4];
        pointNode->getDefaultPointColor(defaultColor);
        QVariantList points;
        points.reserve(pointNode->getPointCount());
        for (int index = 0; index < pointNode->getPointCount(); ++index) {
            const PointItem& point = pointNode->getPointByIndex(index);
            points.append(QVariantMap{
                {QStringLiteral("label"), point.label},
                {QStringLiteral("position"), QVariantList{point.position[0], point.position[1], point.position[2]}},
                {QStringLiteral("selected"), point.selectedFlag},
                {QStringLiteral("visible"), point.visibleFlag},
                {QStringLiteral("locked"), point.lockedFlag},
                {QStringLiteral("associatedNodeId"), point.associatedNodeId},
                {QStringLiteral("color"), QVariantList{point.colorRGBA[0], point.colorRGBA[1], point.colorRGBA[2], point.colorRGBA[3]}},
                {QStringLiteral("sizeValue"), point.sizeValue}
            });
        }

        payload.insert(QStringLiteral("pointRole"), pointNode->getPointRole());
        payload.insert(QStringLiteral("showLabels"), pointNode->isShowPointLabel());
        payload.insert(QStringLiteral("pointLabelFormat"), pointNode->getPointLabelFormat());
        payload.insert(QStringLiteral("selectedPointIndex"), pointNode->getSelectedPointIndex());
        payload.insert(QStringLiteral("defaultPointSize"), pointNode->getDefaultPointSize());
        payload.insert(QStringLiteral("defaultPointColor"), QVariantList{defaultColor[0], defaultColor[1], defaultColor[2], defaultColor[3]});
        payload.insert(QStringLiteral("points"), points);
    } else if (auto* lineNode = dynamic_cast<LineNode*>(node)) {
        double color[4];
        lineNode->getColor(color);
        payload.insert(QStringLiteral("lineRole"), lineNode->getLineRole());
        payload.insert(QStringLiteral("closed"), lineNode->isClosed());
        payload.insert(QStringLiteral("renderMode"), lineNode->getRenderMode());
        payload.insert(QStringLiteral("dashed"), lineNode->isDashed());
        payload.insert(QStringLiteral("opacity"), lineNode->getOpacity());
        payload.insert(QStringLiteral("lineWidth"), lineNode->getLineWidth());
        payload.insert(QStringLiteral("color"), QVariantList{color[0], color[1], color[2], color[3]});
        QVariantList vertices;
        vertices.reserve(lineNode->getVertexCount());
        for (int index = 0; index < lineNode->getVertexCount(); ++index) {
            const auto vertex = lineNode->getVertex(index);
            vertices.append(QVariantList{vertex[0], vertex[1], vertex[2]});
        }
        payload.insert(QStringLiteral("vertices"), vertices);
    } else if (auto* modelNode = dynamic_cast<ModelNode*>(node)) {
        double color[4];
        double edgeColor[4];
        modelNode->getColor(color);
        modelNode->getEdgeColor(edgeColor);
        payload.insert(QStringLiteral("modelRole"), modelNode->getModelRole());
        payload.insert(QStringLiteral("geometryPreset"), modelNode->getAttribute(QStringLiteral("geometryPreset")).toString());
        payload.insert(QStringLiteral("renderMode"), modelNode->getRenderMode());
        payload.insert(QStringLiteral("opacity"), modelNode->getOpacity());
        payload.insert(QStringLiteral("showEdges"), modelNode->isShowEdges());
        payload.insert(QStringLiteral("edgeWidth"), modelNode->getEdgeWidth());
        payload.insert(QStringLiteral("backfaceCulling"), modelNode->isBackfaceCulling());
        payload.insert(QStringLiteral("useScalarColor"), modelNode->isUseScalarColor());
        payload.insert(QStringLiteral("scalarColorMap"), modelNode->getScalarColorMap());
        payload.insert(QStringLiteral("color"), QVariantList{color[0], color[1], color[2], color[3]});
        payload.insert(QStringLiteral("edgeColor"), QVariantList{edgeColor[0], edgeColor[1], edgeColor[2], edgeColor[3]});
        payload.insert(QStringLiteral("vertices"), toPointVariantList(modelNode->getVertices()));
        payload.insert(QStringLiteral("triangles"), toTriangleVariantList(modelNode->getIndices()));
    } else if (auto* transformNode = dynamic_cast<TransformNode*>(node)) {
        double matrix[16];
        double colorX[4];
        double colorY[4];
        double colorZ[4];
        transformNode->getMatrixTransformToParent(matrix);
        transformNode->getAxesColorX(colorX);
        transformNode->getAxesColorY(colorY);
        transformNode->getAxesColorZ(colorZ);
        payload.insert(QStringLiteral("transformKind"), transformNode->getTransformKind());
        payload.insert(QStringLiteral("sourceSpace"), transformNode->getSourceSpace());
        payload.insert(QStringLiteral("targetSpace"), transformNode->getTargetSpace());
        payload.insert(QStringLiteral("showAxes"), transformNode->isShowAxes());
        payload.insert(QStringLiteral("axesLength"), transformNode->getAxesLength());
        payload.insert(QStringLiteral("axesColorX"), QVariantList{colorX[0], colorX[1], colorX[2], colorX[3]});
        payload.insert(QStringLiteral("axesColorY"), QVariantList{colorY[0], colorY[1], colorY[2], colorY[3]});
        payload.insert(QStringLiteral("axesColorZ"), QVariantList{colorZ[0], colorZ[1], colorZ[2], colorZ[3]});
        payload.insert(QStringLiteral("matrix"), toMatrixVariantList(matrix));
        payload.insert(QStringLiteral("pose"), QVariantMap{
            {QStringLiteral("tx"), attributeAsDouble(node, QStringLiteral("poseTx"))},
            {QStringLiteral("ty"), attributeAsDouble(node, QStringLiteral("poseTy"))},
            {QStringLiteral("tz"), attributeAsDouble(node, QStringLiteral("poseTz"))},
            {QStringLiteral("rx"), attributeAsDouble(node, QStringLiteral("poseRx"))},
            {QStringLiteral("ry"), attributeAsDouble(node, QStringLiteral("poseRy"))},
            {QStringLiteral("rz"), attributeAsDouble(node, QStringLiteral("poseRz"))}
        });
    }

    return payload;
}

bool DataGenModuleLogicHandler::restoreFromRedisSnapshot(const QVariantMap& snapshot)
{
    const QVariantList serializedNodes = snapshot.value(QStringLiteral("nodes")).toList();
    SceneGraph* scene = getSceneGraph();
    if (!scene || serializedNodes.isEmpty()) {
        return false;
    }

    const QVector<NodeBase*> existingNodes = managedNodes();
    for (NodeBase* existingNode : existingNodes) {
        scene->removeNode(existingNode->getNodeId());
    }

    QMap<QString, NodeBase*> createdByPersistId;
    QList<QPair<NodeBase*, QString>> pendingParents;

    for (const QVariant& item : serializedNodes) {
        const QVariantMap nodeMap = item.toMap();
        const QString nodeType = nodeMap.value(QStringLiteral("type")).toString();
        const QVariantMap displayMap = nodeMap.value(QStringLiteral("display")).toMap();
        const int layer = displayMap.value(QStringLiteral("layer"), 1).toInt();
        NodeBase* createdNode = nullptr;

        if (nodeType == QStringLiteral("point")) {
            auto* pointNode = new PointNode(scene);
            setManagedDefaults(pointNode, layer);
            pointNode->setName(nodeMap.value(QStringLiteral("name")).toString());
            pointNode->setAttribute(persistIdAttributeName(), nodeMap.value(QStringLiteral("persistId")).toString());
            pointNode->setPointRole(nodeMap.value(QStringLiteral("pointRole")).toString());
            pointNode->setPointLabelFormat(nodeMap.value(QStringLiteral("pointLabelFormat"), QStringLiteral("%1")).toString());
            pointNode->setShowPointLabel(nodeMap.value(QStringLiteral("showLabels"), false).toBool());
            pointNode->setSelectedPointIndex(nodeMap.value(QStringLiteral("selectedPointIndex"), -1).toInt());
            const QVariantList defaultColor = nodeMap.value(QStringLiteral("defaultPointColor")).toList();
            if (defaultColor.size() == 4) {
                const double color[4] = {
                    defaultColor.at(0).toDouble(),
                    defaultColor.at(1).toDouble(),
                    defaultColor.at(2).toDouble(),
                    defaultColor.at(3).toDouble()
                };
                pointNode->setDefaultPointColor(color);
            }
            pointNode->setDefaultPointSize(nodeMap.value(QStringLiteral("defaultPointSize"), 8.0).toDouble());
            for (const QVariant& pointValue : nodeMap.value(QStringLiteral("points")).toList()) {
                const QVariantMap pointMap = pointValue.toMap();
                PointItem pointItem;
                pointItem.label = pointMap.value(QStringLiteral("label")).toString();
                const QVariantList pos = pointMap.value(QStringLiteral("position")).toList();
                if (pos.size() >= 3) {
                    pointItem.position[0] = pos.at(0).toDouble();
                    pointItem.position[1] = pos.at(1).toDouble();
                    pointItem.position[2] = pos.at(2).toDouble();
                }
                pointItem.selectedFlag = pointMap.value(QStringLiteral("selected"), false).toBool();
                pointItem.visibleFlag = pointMap.value(QStringLiteral("visible"), true).toBool();
                pointItem.lockedFlag = pointMap.value(QStringLiteral("locked"), false).toBool();
                pointItem.associatedNodeId = pointMap.value(QStringLiteral("associatedNodeId")).toString();
                const QVariantList pointColor = pointMap.value(QStringLiteral("color")).toList();
                if (pointColor.size() == 4) {
                    for (int index = 0; index < 4; ++index) {
                        pointItem.colorRGBA[index] = pointColor.at(index).toDouble();
                    }
                }
                pointItem.sizeValue = pointMap.value(QStringLiteral("sizeValue"), -1.0).toDouble();
                pointNode->addPoint(pointItem);
            }
            createdNode = pointNode;
        } else if (nodeType == QStringLiteral("line")) {
            auto* lineNode = new LineNode(scene);
            setManagedDefaults(lineNode, layer);
            lineNode->setName(nodeMap.value(QStringLiteral("name")).toString());
            lineNode->setAttribute(persistIdAttributeName(), nodeMap.value(QStringLiteral("persistId")).toString());
            lineNode->setLineRole(nodeMap.value(QStringLiteral("lineRole")).toString());
            const QVariantList color = nodeMap.value(QStringLiteral("color")).toList();
            if (color.size() == 4) {
                const double rgba[4] = {
                    color.at(0).toDouble(),
                    color.at(1).toDouble(),
                    color.at(2).toDouble(),
                    color.at(3).toDouble()
                };
                lineNode->setColor(rgba);
            }
            lineNode->setOpacity(nodeMap.value(QStringLiteral("opacity"), 1.0).toDouble());
            lineNode->setLineWidth(nodeMap.value(QStringLiteral("lineWidth"), 4.0).toDouble());
            lineNode->setRenderMode(nodeMap.value(QStringLiteral("renderMode"), QStringLiteral("surface")).toString());
            lineNode->setDashed(nodeMap.value(QStringLiteral("dashed"), false).toBool());
            lineNode->setClosed(nodeMap.value(QStringLiteral("closed"), false).toBool());
            lineNode->setPolyline(fromPointVariantList(nodeMap.value(QStringLiteral("vertices")).toList()));
            createdNode = lineNode;
        } else if (nodeType == QStringLiteral("model")) {
            auto* modelNode = new ModelNode(scene);
            setManagedDefaults(modelNode, layer);
            modelNode->setName(nodeMap.value(QStringLiteral("name")).toString());
            modelNode->setAttribute(persistIdAttributeName(), nodeMap.value(QStringLiteral("persistId")).toString());
            modelNode->setModelRole(nodeMap.value(QStringLiteral("modelRole")).toString());
            modelNode->setAttribute(QStringLiteral("geometryPreset"), nodeMap.value(QStringLiteral("geometryPreset")).toString());
            const QVariantList color = nodeMap.value(QStringLiteral("color")).toList();
            if (color.size() == 4) {
                const double rgba[4] = {
                    color.at(0).toDouble(),
                    color.at(1).toDouble(),
                    color.at(2).toDouble(),
                    color.at(3).toDouble()
                };
                modelNode->setColor(rgba);
            }
            const QVariantList edgeColor = nodeMap.value(QStringLiteral("edgeColor")).toList();
            if (edgeColor.size() == 4) {
                const double rgba[4] = {
                    edgeColor.at(0).toDouble(),
                    edgeColor.at(1).toDouble(),
                    edgeColor.at(2).toDouble(),
                    edgeColor.at(3).toDouble()
                };
                modelNode->setEdgeColor(rgba);
            }
            modelNode->setOpacity(nodeMap.value(QStringLiteral("opacity"), 1.0).toDouble());
            modelNode->setRenderMode(nodeMap.value(QStringLiteral("renderMode"), QStringLiteral("surface")).toString());
            modelNode->setShowEdges(nodeMap.value(QStringLiteral("showEdges"), false).toBool());
            modelNode->setEdgeWidth(nodeMap.value(QStringLiteral("edgeWidth"), 1.0).toDouble());
            modelNode->setBackfaceCulling(nodeMap.value(QStringLiteral("backfaceCulling"), false).toBool());
            modelNode->setUseScalarColor(nodeMap.value(QStringLiteral("useScalarColor"), false).toBool());
            modelNode->setScalarColorMap(nodeMap.value(QStringLiteral("scalarColorMap")).toString());
            modelNode->setMeshData(
                fromPointVariantList(nodeMap.value(QStringLiteral("vertices")).toList()),
                fromTriangleVariantList(nodeMap.value(QStringLiteral("triangles")).toList()));
            createdNode = modelNode;
        } else if (nodeType == QStringLiteral("transform")) {
            auto* transformNode = new TransformNode(scene);
            setManagedDefaults(transformNode, layer);
            transformNode->setName(nodeMap.value(QStringLiteral("name")).toString());
            transformNode->setAttribute(persistIdAttributeName(), nodeMap.value(QStringLiteral("persistId")).toString());
            transformNode->setTransformKind(nodeMap.value(QStringLiteral("transformKind")).toString());
            transformNode->setSourceSpace(nodeMap.value(QStringLiteral("sourceSpace")).toString());
            transformNode->setTargetSpace(nodeMap.value(QStringLiteral("targetSpace")).toString());
            transformNode->setShowAxes(nodeMap.value(QStringLiteral("showAxes"), true).toBool());
            transformNode->setAxesLength(nodeMap.value(QStringLiteral("axesLength"), 60.0).toDouble());
            const QVariantList axesColorX = nodeMap.value(QStringLiteral("axesColorX")).toList();
            const QVariantList axesColorY = nodeMap.value(QStringLiteral("axesColorY")).toList();
            const QVariantList axesColorZ = nodeMap.value(QStringLiteral("axesColorZ")).toList();
            if (axesColorX.size() == 4) {
                const double rgba[4] = {
                    axesColorX.at(0).toDouble(),
                    axesColorX.at(1).toDouble(),
                    axesColorX.at(2).toDouble(),
                    axesColorX.at(3).toDouble()
                };
                transformNode->setAxesColorX(rgba);
            }
            if (axesColorY.size() == 4) {
                const double rgba[4] = {
                    axesColorY.at(0).toDouble(),
                    axesColorY.at(1).toDouble(),
                    axesColorY.at(2).toDouble(),
                    axesColorY.at(3).toDouble()
                };
                transformNode->setAxesColorY(rgba);
            }
            if (axesColorZ.size() == 4) {
                const double rgba[4] = {
                    axesColorZ.at(0).toDouble(),
                    axesColorZ.at(1).toDouble(),
                    axesColorZ.at(2).toDouble(),
                    axesColorZ.at(3).toDouble()
                };
                transformNode->setAxesColorZ(rgba);
            }
            double matrix[16];
            if (fillMatrixFromVariantList(nodeMap.value(QStringLiteral("matrix")).toList(), matrix)) {
                transformNode->setMatrixTransformToParent(matrix);
            }
            const QVariantMap poseMap = nodeMap.value(QStringLiteral("pose")).toMap();
            transformNode->setAttribute(QStringLiteral("poseTx"), poseMap.value(QStringLiteral("tx"), 0.0));
            transformNode->setAttribute(QStringLiteral("poseTy"), poseMap.value(QStringLiteral("ty"), 0.0));
            transformNode->setAttribute(QStringLiteral("poseTz"), poseMap.value(QStringLiteral("tz"), 0.0));
            transformNode->setAttribute(QStringLiteral("poseRx"), poseMap.value(QStringLiteral("rx"), 0.0));
            transformNode->setAttribute(QStringLiteral("poseRy"), poseMap.value(QStringLiteral("ry"), 0.0));
            transformNode->setAttribute(QStringLiteral("poseRz"), poseMap.value(QStringLiteral("rz"), 0.0));
            createdNode = transformNode;
        }

        if (!createdNode) {
            continue;
        }

        DisplayTarget displayTarget;
        displayTarget.visible = displayMap.value(QStringLiteral("visible"), true).toBool();
        displayTarget.layer = qBound(1, displayMap.value(QStringLiteral("layer"), 1).toInt(), 3);
        createdNode->setWindowDisplayTarget(QStringLiteral("datagen_main"), displayTarget);
        scene->addNode(createdNode);

        const QString persistId = nodeMap.value(QStringLiteral("persistId")).toString();
        createdByPersistId.insert(persistId, createdNode);
        pendingParents.append({createdNode, nodeMap.value(QStringLiteral("parentPersistId")).toString()});
    }

    for (const auto& pending : pendingParents) {
        if (!pending.first || pending.second.isEmpty()) {
            continue;
        }
        NodeBase* parentNode = createdByPersistId.value(pending.second, nullptr);
        if (parentNode) {
            assignParent(pending.first, parentNode->getNodeId());
        }
    }

    const QString selectedPersistId = snapshot.value(QStringLiteral("selectedNodePersistId")).toString();
    m_selectedNodeId = createdByPersistId.contains(selectedPersistId)
        ? createdByPersistId.value(selectedPersistId)->getNodeId()
        : QString();
    selectFallbackNode();

    const QString restoredStatusText = snapshot.value(QStringLiteral("statusText")).toString();
    if (!restoredStatusText.isEmpty()) {
        m_statusText = restoredStatusText;
    }
    return !createdByPersistId.isEmpty();
}

QVector<NodeBase*> DataGenModuleLogicHandler::managedNodes() const
{
    QVector<NodeBase*> result;
    SceneGraph* scene = getSceneGraph();
    if (!scene) {
        return result;
    }

    for (NodeBase* node : scene->getAllNodes()) {
        if (node->getAttribute(QStringLiteral("ownerModule")).toString() == moduleOwnerTag()) {
            result.append(node);
        }
    }
    return result;
}

QVector<TransformNode*> DataGenModuleLogicHandler::managedTransforms() const
{
    QVector<TransformNode*> result;
    for (NodeBase* node : managedNodes()) {
        if (auto* transformNode = dynamic_cast<TransformNode*>(node)) {
            result.append(transformNode);
        }
    }
    return result;
}

NodeBase* DataGenModuleLogicHandler::nodeById(const QString& nodeId) const
{
    if (nodeId.isEmpty()) {
        return nullptr;
    }

    SceneGraph* scene = getSceneGraph();
    NodeBase* node = scene ? scene->getNodeById(nodeId) : nullptr;
    if (!node) {
        return nullptr;
    }
    return node->getAttribute(QStringLiteral("ownerModule")).toString() == moduleOwnerTag()
        ? node
        : nullptr;
}

TransformNode* DataGenModuleLogicHandler::transformById(const QString& nodeId) const
{
    return dynamic_cast<TransformNode*>(nodeById(nodeId));
}

void DataGenModuleLogicHandler::setManagedDefaults(NodeBase* node, int layer) const
{
    if (!node) {
        return;
    }

    node->setAttribute(QStringLiteral("ownerModule"), moduleOwnerTag());
    if (node->getAttribute(persistIdAttributeName()).toString().isEmpty()) {
        node->setAttribute(persistIdAttributeName(),
                           QUuid::createUuid().toString(QUuid::WithoutBraces));
    }
    DisplayTarget defaultTarget;
    defaultTarget.visible = false;
    defaultTarget.layer = layer;
    node->setDefaultDisplayTarget(defaultTarget);

    DisplayTarget dataGenTarget;
    dataGenTarget.visible = true;
    dataGenTarget.layer = layer;
    node->setWindowDisplayTarget(QStringLiteral("datagen_main"), dataGenTarget);
}

void DataGenModuleLogicHandler::removeParentReferencesTo(const QString& nodeId)
{
    if (nodeId.isEmpty()) {
        return;
    }

    for (NodeBase* node : managedNodes()) {
        if (parentTransformId(node) == nodeId) {
            if (auto* pointNode = dynamic_cast<PointNode*>(node)) {
                pointNode->setParentTransform(QString());
            } else if (auto* lineNode = dynamic_cast<LineNode*>(node)) {
                lineNode->setParentTransform(QString());
            } else if (auto* modelNode = dynamic_cast<ModelNode*>(node)) {
                modelNode->setParentTransform(QString());
            } else if (auto* transformNode = dynamic_cast<TransformNode*>(node)) {
                transformNode->setParentTransform(QString());
            }
        }
    }
}

void DataGenModuleLogicHandler::selectFallbackNode()
{
    if (nodeById(m_selectedNodeId)) {
        return;
    }

    const QVector<NodeBase*> nodes = managedNodes();
    m_selectedNodeId = nodes.isEmpty() ? QString() : nodes.first()->getNodeId();
}

PointNode* DataGenModuleLogicHandler::createPointNode(const QVariantMap& payload)
{
    SceneGraph* scene = getSceneGraph();
    if (!scene) {
        return nullptr;
    }

    auto* node = new PointNode(scene);
    node->setName(payload.value(QStringLiteral("name"), QStringLiteral("Generated Points")).toString());
    setManagedDefaults(node, 3);
    node->setPointRole(QStringLiteral("generated_landmarks"));
    const double color[4] = {0.99, 0.57, 0.18, 1.0};
    node->setDefaultPointColor(color);
    node->setDefaultPointSize(8.0);

    const int count = qMax(1, payload.value(QStringLiteral("count"), 5).toInt());
    const double spacing = qMax(1.0, payload.value(QStringLiteral("spacing"), 16.0).toDouble());
    for (int index = 0; index < count; ++index) {
        PointItem point;
        point.label = QStringLiteral("P%1").arg(index + 1);
        point.position[0] = index * spacing;
        point.position[1] = (index % 2 == 0) ? 0.0 : spacing * 0.5;
        point.position[2] = index * 3.0;
        node->addPoint(point);
    }

    scene->addNode(node);
    return node;
}

LineNode* DataGenModuleLogicHandler::createLineNode(const QVariantMap& payload)
{
    SceneGraph* scene = getSceneGraph();
    if (!scene) {
        return nullptr;
    }

    auto* node = new LineNode(scene);
    node->setName(payload.value(QStringLiteral("name"), QStringLiteral("Generated Path")).toString());
    setManagedDefaults(node, 3);
    node->setLineRole(QStringLiteral("generated_path"));
    const double color[4] = {0.16, 0.82, 0.67, 1.0};
    node->setColor(color);
    node->setOpacity(1.0);
    node->setLineWidth(4.0);
    node->setRenderMode(QStringLiteral("surface"));

    QVector<std::array<double, 3>> vertices;
    const int count = qMax(2, payload.value(QStringLiteral("count"), 4).toInt());
    const double spacing = qMax(1.0, payload.value(QStringLiteral("spacing"), 24.0).toDouble());
    vertices.reserve(count);
    for (int index = 0; index < count; ++index) {
        vertices.push_back({
            index * spacing,
            qSin(index * 0.7) * spacing * 0.55,
            qCos(index * 0.4) * spacing * 0.25
        });
    }
    node->setPolyline(vertices);
    node->setClosed(payload.value(QStringLiteral("closed"), false).toBool());
    scene->addNode(node);
    return node;
}

ModelNode* DataGenModuleLogicHandler::createModelNode(const QVariantMap& payload)
{
    SceneGraph* scene = getSceneGraph();
    if (!scene) {
        return nullptr;
    }

    auto* node = new ModelNode(scene);
    node->setName(payload.value(QStringLiteral("name"), QStringLiteral("Generated Model")).toString());
    setManagedDefaults(node, 1);
    const QString shape = payload.value(QStringLiteral("shape"), QStringLiteral("sphere")).toString();
    node->setAttribute(QStringLiteral("geometryPreset"), shape);
    const double sizeA = qMax(1.0, payload.value(QStringLiteral("sizeA"), 30.0).toDouble());
    const double sizeB = qMax(1.0, payload.value(QStringLiteral("sizeB"), sizeA).toDouble());
    const double sizeC = qMax(1.0, payload.value(QStringLiteral("sizeC"), sizeA).toDouble());
    const int resolution = qMax(6, payload.value(QStringLiteral("resolution"), 24).toInt());
    node->setPolyData(buildShapePolyData(shape, sizeA, sizeB, sizeC, resolution));
    const double color[4] = {0.33, 0.58, 0.92, 0.85};
    node->setColor(color);
    node->setOpacity(0.85);
    node->setRenderMode(QStringLiteral("surface"));
    node->setShowEdges(true);
    const double edgeColor[4] = {0.04, 0.1, 0.22, 1.0};
    node->setEdgeColor(edgeColor);
    node->setEdgeWidth(1.2);
    scene->addNode(node);
    return node;
}

TransformNode* DataGenModuleLogicHandler::createTransformNode(const QVariantMap& payload)
{
    SceneGraph* scene = getSceneGraph();
    if (!scene) {
        return nullptr;
    }

    auto* node = new TransformNode(scene);
    node->setName(payload.value(QStringLiteral("name"), QStringLiteral("Generated Transform")).toString());
    setManagedDefaults(node, 3);
    node->setTransformKind(QStringLiteral("rigid"));
    node->setSourceSpace(QStringLiteral("local"));
    node->setTargetSpace(QStringLiteral("world"));
    node->setShowAxes(payload.value(QStringLiteral("showAxes"), true).toBool());
    node->setAxesLength(payload.value(QStringLiteral("axesLength"), 60.0).toDouble());
    const double colorX[4] = {1.0, 0.22, 0.22, 1.0};
    const double colorY[4] = {0.18, 0.86, 0.26, 1.0};
    const double colorZ[4] = {0.18, 0.56, 0.98, 1.0};
    node->setAxesColorX(colorX);
    node->setAxesColorY(colorY);
    node->setAxesColorZ(colorZ);
    updateTransformPose(node, {});
    scene->addNode(node);
    return node;
}

void DataGenModuleLogicHandler::updateDisplay(NodeBase* node, const QVariantMap& payload)
{
    if (!node) {
        return;
    }

    DisplayTarget target;
    target.visible = payload.value(QStringLiteral("visible"), true).toBool();
    target.layer = qBound(1, payload.value(QStringLiteral("layer"), 1).toInt(), 3);
    node->setWindowDisplayTarget(QStringLiteral("datagen_main"), target);

    const double red = payload.value(QStringLiteral("red"), 1.0).toDouble();
    const double green = payload.value(QStringLiteral("green"), 1.0).toDouble();
    const double blue = payload.value(QStringLiteral("blue"), 1.0).toDouble();
    const double opacity = payload.value(QStringLiteral("opacity"), 1.0).toDouble();

    if (auto* pointNode = dynamic_cast<PointNode*>(node)) {
        const double color[4] = {red, green, blue, opacity};
        pointNode->setDefaultPointColor(color);
        pointNode->setDefaultPointSize(payload.value(QStringLiteral("sizeValue"), 6.0).toDouble());
        pointNode->setShowPointLabel(payload.value(QStringLiteral("showLabels"), false).toBool());
    } else if (auto* lineNode = dynamic_cast<LineNode*>(node)) {
        const double color[4] = {red, green, blue, opacity};
        lineNode->setColor(color);
        lineNode->setOpacity(opacity);
        lineNode->setLineWidth(payload.value(QStringLiteral("sizeValue"), 4.0).toDouble());
        lineNode->setRenderMode(payload.value(QStringLiteral("renderMode"), QStringLiteral("surface")).toString());
        lineNode->setDashed(payload.value(QStringLiteral("dashed"), false).toBool());
    } else if (auto* modelNode = dynamic_cast<ModelNode*>(node)) {
        const double color[4] = {red, green, blue, opacity};
        modelNode->setColor(color);
        modelNode->setOpacity(opacity);
        modelNode->setRenderMode(payload.value(QStringLiteral("renderMode"), QStringLiteral("surface")).toString());
        modelNode->setShowEdges(payload.value(QStringLiteral("showEdges"), false).toBool());
    } else if (auto* transformNode = dynamic_cast<TransformNode*>(node)) {
        const double axisColor[4] = {red, green, blue, opacity};
        transformNode->setAxesColorX(axisColor);
        transformNode->setAxesColorY(axisColor);
        transformNode->setAxesColorZ(axisColor);
        transformNode->setShowAxes(payload.value(QStringLiteral("showAxes"), false).toBool());
        transformNode->setAxesLength(payload.value(QStringLiteral("sizeValue"), 60.0).toDouble());
    }
}

void DataGenModuleLogicHandler::assignParent(NodeBase* node, const QString& parentTransformId)
{
    if (!node) {
        return;
    }

    if (!parentTransformId.isEmpty() && !transformById(parentTransformId)) {
        return;
    }

    if (auto* pointNode = dynamic_cast<PointNode*>(node)) {
        pointNode->setParentTransform(parentTransformId);
    } else if (auto* lineNode = dynamic_cast<LineNode*>(node)) {
        lineNode->setParentTransform(parentTransformId);
    } else if (auto* modelNode = dynamic_cast<ModelNode*>(node)) {
        modelNode->setParentTransform(parentTransformId);
    } else if (auto* transformNode = dynamic_cast<TransformNode*>(node)) {
        transformNode->setParentTransform(parentTransformId);
    }
}

void DataGenModuleLogicHandler::updateTransformPose(TransformNode* node, const QVariantMap& payload)
{
    if (!node) {
        return;
    }

    const double tx = payload.value(QStringLiteral("tx"), 0.0).toDouble();
    const double ty = payload.value(QStringLiteral("ty"), 0.0).toDouble();
    const double tz = payload.value(QStringLiteral("tz"), 0.0).toDouble();
    const double rx = payload.value(QStringLiteral("rx"), 0.0).toDouble();
    const double ry = payload.value(QStringLiteral("ry"), 0.0).toDouble();
    const double rz = payload.value(QStringLiteral("rz"), 0.0).toDouble();
    double matrix[16];
    buildPoseMatrix(tx, ty, tz, rx, ry, rz, matrix);
    node->setMatrixTransformToParent(matrix);
    node->setAttribute(QStringLiteral("poseTx"), tx);
    node->setAttribute(QStringLiteral("poseTy"), ty);
    node->setAttribute(QStringLiteral("poseTz"), tz);
    node->setAttribute(QStringLiteral("poseRx"), rx);
    node->setAttribute(QStringLiteral("poseRy"), ry);
    node->setAttribute(QStringLiteral("poseRz"), rz);
}

void DataGenModuleLogicHandler::clearNodeGeometry(NodeBase* node)
{
    if (auto* pointNode = dynamic_cast<PointNode*>(node)) {
        pointNode->removeAllPoints();
        return;
    }
    if (auto* lineNode = dynamic_cast<LineNode*>(node)) {
        lineNode->clearVertices();
        return;
    }
    if (auto* modelNode = dynamic_cast<ModelNode*>(node)) {
        modelNode->clearPolyData();
        return;
    }
    if (auto* transformNode = dynamic_cast<TransformNode*>(node)) {
        updateTransformPose(transformNode, {});
    }
}

bool DataGenModuleLogicHandler::deleteNode(const QString& nodeId)
{
    SceneGraph* scene = getSceneGraph();
    if (!scene || !nodeById(nodeId)) {
        return false;
    }

    removeParentReferencesTo(nodeId);
    if (m_selectedNodeId == nodeId) {
        m_selectedNodeId.clear();
    }
    const bool removed = scene->removeNode(nodeId);
    selectFallbackNode();
    return removed;
}

void DataGenModuleLogicHandler::persistRedisSnapshot(const QString& changeEvent,
                                                     const QString& changedNodePersistId,
                                                     const QString& changedNodeName)
{
    if (!hasRedisCommandAccess()) {
        return;
    }

    const QVariantMap snapshot = buildRedisSnapshot(
        changeEvent,
        changedNodePersistId,
        changedNodeName);
    writeRedisJsonValue(dataGenStateRedisKey(), snapshot);
    publishRedisJsonMessage(dataGenStateRedisChannel(), snapshot);
}