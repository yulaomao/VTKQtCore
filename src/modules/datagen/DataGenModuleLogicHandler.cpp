#include "DataGenModuleLogicHandler.h"

#include "logic/scene/SceneGraph.h"
#include "logic/scene/nodes/LineNode.h"
#include "logic/scene/nodes/ModelNode.h"
#include "logic/scene/nodes/NodeBase.h"
#include "logic/scene/nodes/PlaneNode.h"
#include "logic/scene/nodes/PointNode.h"
#include "logic/scene/nodes/TransformNode.h"
#include "contracts/PromptAudioPresetIds.h"

#include <vtkCubeSource.h>
#include <vtkCylinderSource.h>
#include <vtkConeSource.h>
#include <vtkPolyData.h>
#include <vtkSphereSource.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>
#include <vtkMatrix4x4.h>

#include <QTimer>
#include <QUuid>

#include <QtMath>

namespace {

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
    if (dynamic_cast<const PlaneNode*>(node)) {
        return QStringLiteral("plane");
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

void applyModelMaterialPayload(ModelNode* modelNode, const QVariantMap& payload)
{
    if (!modelNode) {
        return;
    }

    modelNode->setMaterialAmbient(
        qBound(0.0, payload.value(QStringLiteral("ambient"), 0.2).toDouble(), 1.0));
    modelNode->setMaterialDiffuse(
        qBound(0.0, payload.value(QStringLiteral("diffuse"), 0.8).toDouble(), 1.0));
    modelNode->setMaterialSpecular(
        qBound(0.0, payload.value(QStringLiteral("specular"), 0.15).toDouble(), 1.0));
    modelNode->setMaterialSpecularPower(
        qMax(0.0,
             payload.value(
                 QStringLiteral("specularPower"),
                 payload.value(QStringLiteral("power"), 20.0)).toDouble()));
    modelNode->setMaterialRoughness(
        qBound(0.0, payload.value(QStringLiteral("roughness"), 0.4).toDouble(), 1.0));
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

}

DataGenModuleLogicHandler::DataGenModuleLogicHandler(QObject* parent)
    : ModuleLogicHandler(QStringLiteral("datagen"), parent)
    , m_promptBurstTimer(new QTimer(this))
{
    m_promptBurstTimer->setSingleShot(false);
    connect(m_promptBurstTimer, &QTimer::timeout,
            this, [this]() {
                if (m_promptBurstRemaining <= 0 || m_promptBurstPresetId.isEmpty()) {
                    stopPromptBurst();
                    return;
                }

                playPromptAudioPreset(m_promptBurstPresetId);
                --m_promptBurstRemaining;
                if (m_promptBurstRemaining <= 0) {
                    stopPromptBurst();
                }
            });
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
        } else if (nodeType == QStringLiteral("plane")) {
            created = createPlaneNode(request.payload);
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
    ensureSeedScene();

    emitState(m_statusText);
}

void DataGenModuleLogicHandler::onResync()
{
    emitState(QStringLiteral("DataGen 模块已重同步。"));
}

void DataGenModuleLogicHandler::stopPromptBurst()
{
    if (m_promptBurstTimer) {
        m_promptBurstTimer->stop();
    }
    m_promptBurstPresetId.clear();
    m_promptBurstSourceActionId.clear();
    m_promptBurstRemaining = 0;
}

void DataGenModuleLogicHandler::playPromptPresetBurst(const QString& presetId,
                                                      int count,
                                                      int intervalMs,
                                                      const QString& sourceActionId)
{
    const QString normalizedPresetId = presetId.trimmed();
    if (normalizedPresetId.isEmpty()) {
        emitState(QStringLiteral("提示音预设不能为空。"),
                  LogicNotification::SceneNodesUpdated,
                  sourceActionId);
        return;
    }

    const int safeCount = qBound(1, count, 100);
    const int safeIntervalMs = qBound(10, intervalMs, 2000);

    stopPromptBurst();
    m_promptBurstPresetId = normalizedPresetId;
    m_promptBurstSourceActionId = sourceActionId;
    m_promptBurstRemaining = safeCount;

    playPromptAudioPreset(m_promptBurstPresetId);
    --m_promptBurstRemaining;
    if (m_promptBurstRemaining <= 0) {
        stopPromptBurst();
        return;
    }

    m_promptBurstTimer->start(safeIntervalMs);
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
    auto* planeNode = createPlaneNode({
        {QStringLiteral("name"), QStringLiteral("Reference Plane")},
        {QStringLiteral("width"), 72.0},
        {QStringLiteral("height"), 48.0},
        {QStringLiteral("centerX"), 24.0},
        {QStringLiteral("centerY"), -18.0},
        {QStringLiteral("centerZ"), 8.0},
        {QStringLiteral("normalX"), 0.0},
        {QStringLiteral("normalY"), 0.25},
        {QStringLiteral("normalZ"), 1.0}
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
    if (planeNode) {
        assignParent(planeNode, rootTransform->getNodeId());
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

    if (command == QStringLiteral("test_prompt_play_once")) {
        const QString presetId = payload.value(QStringLiteral("presetId")).toString();
        const bool ok = playPromptAudioPreset(presetId);
        emitState(ok
                      ? QStringLiteral("已触发预设提示音：%1。").arg(presetId)
                      : QStringLiteral("提示音触发失败：%1。").arg(presetId),
                  LogicNotification::SceneNodesUpdated,
                  QString());
        return;
    }

    if (command == QStringLiteral("test_prompt_play_burst")) {
        const QString presetId = payload.value(QStringLiteral("presetId")).toString();
        const int count = payload.value(QStringLiteral("count"), 10).toInt();
        const int intervalMs = payload.value(QStringLiteral("intervalMs"), 100).toInt();
        playPromptPresetBurst(presetId, count, intervalMs, sourceActionId);
        emitState(QStringLiteral("已开始高频提示音测试：%1，每 %2ms 一次，共 %3 次。")
                      .arg(presetId)
                      .arg(intervalMs)
                      .arg(count),
                  LogicNotification::SceneNodesUpdated,
                  QString());
        return;
    }

    if (command == QStringLiteral("seed_demo")) {
        ensureSeedScene();
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
        } else if (nodeType == QStringLiteral("plane")) {
            created = createPlaneNode(payload);
        } else if (nodeType == QStringLiteral("transform")) {
            created = createTransformNode(payload);
        }

        if (created) {
            m_selectedNodeId = created->getNodeId();
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
        if (deleteNode(node->getNodeId())) {
            emitState(QStringLiteral("已删除 %1。").arg(deletedName),
                      LogicNotification::SceneNodesUpdated,
                      sourceActionId);
        }
        return;
    }

    if (command == QStringLiteral("clear_node_geometry")) {
        clearNodeGeometry(node);
        emitState(QStringLiteral("已清空节点数据。"),
                  LogicNotification::SceneNodesUpdated,
                  sourceActionId);
        return;
    }

    if (command == QStringLiteral("update_display")) {
        updateDisplay(node, payload);
        emitState(QStringLiteral("显示属性已更新。"),
                  LogicNotification::SceneNodesUpdated,
                  sourceActionId);
        return;
    }

    if (command == QStringLiteral("assign_parent")) {
        assignParent(node, payload.value(QStringLiteral("parentTransformId")).toString());
        emitState(QStringLiteral("父变换关系已更新。"),
                  LogicNotification::SceneNodesUpdated,
                  sourceActionId);
        return;
    }

    if (command == QStringLiteral("update_transform_pose")) {
        if (auto* transformNode = dynamic_cast<TransformNode*>(node)) {
            updateTransformPose(transformNode, payload);
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
            emitState(QStringLiteral("已向 LineNode 添加顶点。"),
                      LogicNotification::SceneNodesUpdated,
                      sourceActionId);
        }
        return;
    }

    if (command == QStringLiteral("update_plane_geometry")) {
        if (auto* planeNode = dynamic_cast<PlaneNode*>(node)) {
            const std::array<double, 3> center = planeNode->getCenter();
            const std::array<double, 3> normal = planeNode->getNormal();
            planeNode->setPlaneSize(
                payload.value(QStringLiteral("width"), planeNode->getPlaneWidth()).toDouble(),
                payload.value(QStringLiteral("height"), planeNode->getPlaneHeight()).toDouble());
            planeNode->setCenter(
                payload.value(QStringLiteral("centerX"), center[0]).toDouble(),
                payload.value(QStringLiteral("centerY"), center[1]).toDouble(),
                payload.value(QStringLiteral("centerZ"), center[2]).toDouble());
            planeNode->setNormal(
                payload.value(QStringLiteral("normalX"), normal[0]).toDouble(),
                payload.value(QStringLiteral("normalY"), normal[1]).toDouble(),
                payload.value(QStringLiteral("normalZ"), normal[2]).toDouble());
            emitState(QStringLiteral("平面几何已更新。"),
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

    if (eventType == LogicNotification::SceneNodesUpdated &&
        !sourceActionId.trimmed().isEmpty()) {
        playPromptAudioPreset(PromptAudioPresetIds::pollingProgress());
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
        details.insert(QStringLiteral("opacity"), pointNode->getOpacity());
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
        details.insert(QStringLiteral("ambient"), modelNode->getMaterialAmbient());
        details.insert(QStringLiteral("diffuse"), modelNode->getMaterialDiffuse());
        details.insert(QStringLiteral("specular"), modelNode->getMaterialSpecular());
        details.insert(QStringLiteral("specularPower"), modelNode->getMaterialSpecularPower());
        details.insert(QStringLiteral("roughness"), modelNode->getMaterialRoughness());
        details.insert(QStringLiteral("triangleCount"), modelNode->getIndices().size());
        details.insert(QStringLiteral("shape"), modelNode->getAttribute(
            QStringLiteral("geometryPreset"), QStringLiteral("mesh")).toString());
    } else if (auto* planeNode = dynamic_cast<PlaneNode*>(node)) {
        const std::array<double, 3> center = planeNode->getCenter();
        const std::array<double, 3> normal = planeNode->getNormal();
        double borderColor[4];
        planeNode->getPlaneColor(color);
        planeNode->getBorderColor(borderColor);
        details.insert(QStringLiteral("opacity"), planeNode->getPlaneOpacity());
        details.insert(QStringLiteral("width"), planeNode->getPlaneWidth());
        details.insert(QStringLiteral("height"), planeNode->getPlaneHeight());
        details.insert(QStringLiteral("centerX"), center[0]);
        details.insert(QStringLiteral("centerY"), center[1]);
        details.insert(QStringLiteral("centerZ"), center[2]);
        details.insert(QStringLiteral("normalX"), normal[0]);
        details.insert(QStringLiteral("normalY"), normal[1]);
        details.insert(QStringLiteral("normalZ"), normal[2]);
        details.insert(QStringLiteral("borderRed"), borderColor[0]);
        details.insert(QStringLiteral("borderGreen"), borderColor[1]);
        details.insert(QStringLiteral("borderBlue"), borderColor[2]);
        details.insert(QStringLiteral("borderOpacity"), planeNode->getBorderOpacity());
        details.insert(QStringLiteral("borderWidth"), planeNode->getBorderWidth());
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
            } else if (auto* planeNode = dynamic_cast<PlaneNode*>(node)) {
                planeNode->setParentTransform(QString());
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
    applyModelMaterialPayload(node, payload);
    const double edgeColor[4] = {0.04, 0.1, 0.22, 1.0};
    node->setEdgeColor(edgeColor);
    node->setEdgeWidth(1.2);
    scene->addNode(node);
    return node;
}

PlaneNode* DataGenModuleLogicHandler::createPlaneNode(const QVariantMap& payload)
{
    SceneGraph* scene = getSceneGraph();
    if (!scene) {
        return nullptr;
    }

    auto* node = new PlaneNode(scene);
    node->setName(payload.value(QStringLiteral("name"), QStringLiteral("Generated Plane")).toString());
    setManagedDefaults(node, 1);
    node->setPlaneSize(
        qMax(0.01, payload.value(QStringLiteral("width"), 48.0).toDouble()),
        qMax(0.01, payload.value(QStringLiteral("height"), 30.0).toDouble()));
    node->setCenter(
        payload.value(QStringLiteral("centerX"), 0.0).toDouble(),
        payload.value(QStringLiteral("centerY"), 0.0).toDouble(),
        payload.value(QStringLiteral("centerZ"), 0.0).toDouble());
    node->setNormal(
        payload.value(QStringLiteral("normalX"), 0.0).toDouble(),
        payload.value(QStringLiteral("normalY"), 0.0).toDouble(),
        payload.value(QStringLiteral("normalZ"), 1.0).toDouble());
    const double color[4] = {
        payload.value(QStringLiteral("red"), 0.28).toDouble(),
        payload.value(QStringLiteral("green"), 0.68).toDouble(),
        payload.value(QStringLiteral("blue"), 0.94).toDouble(),
        payload.value(QStringLiteral("opacity"), 0.35).toDouble()
    };
    node->setPlaneColor(color);
    node->setPlaneOpacity(color[3]);
    const double borderColor[4] = {
        payload.value(QStringLiteral("borderRed"), 0.05).toDouble(),
        payload.value(QStringLiteral("borderGreen"), 0.12).toDouble(),
        payload.value(QStringLiteral("borderBlue"), 0.2).toDouble(),
        payload.value(QStringLiteral("borderOpacity"), 1.0).toDouble()
    };
    node->setBorderColor(borderColor);
    node->setBorderOpacity(borderColor[3]);
    node->setBorderWidth(payload.value(QStringLiteral("borderWidth"), 2.0).toDouble());
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
        double color[4];
        pointNode->getDefaultPointColor(color);
        color[0] = red;
        color[1] = green;
        color[2] = blue;
        pointNode->setDefaultPointColor(color);
        pointNode->setOpacity(opacity);
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
        applyModelMaterialPayload(modelNode, payload);
    } else if (auto* planeNode = dynamic_cast<PlaneNode*>(node)) {
        const double color[4] = {red, green, blue, opacity};
        const double borderColor[4] = {
            payload.value(QStringLiteral("borderRed"), 0.05).toDouble(),
            payload.value(QStringLiteral("borderGreen"), 0.12).toDouble(),
            payload.value(QStringLiteral("borderBlue"), 0.2).toDouble(),
            payload.value(QStringLiteral("borderOpacity"), 1.0).toDouble()
        };
        planeNode->setPlaneColor(color);
        planeNode->setPlaneOpacity(opacity);
        planeNode->setBorderColor(borderColor);
        planeNode->setBorderOpacity(payload.value(QStringLiteral("borderOpacity"), borderColor[3]).toDouble());
        planeNode->setBorderWidth(payload.value(QStringLiteral("borderWidth"), 2.0).toDouble());
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
    } else if (auto* planeNode = dynamic_cast<PlaneNode*>(node)) {
        planeNode->setParentTransform(parentTransformId);
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
    if (auto* planeNode = dynamic_cast<PlaneNode*>(node)) {
        planeNode->setPlaneSize(1.0, 1.0);
        planeNode->setCenter(0.0, 0.0, 0.0);
        planeNode->setNormal(0.0, 0.0, 1.0);
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
