#include "ModelNodeDisplayManager.h"
#include "../logic/scene/nodes/ModelNode.h"

#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkLookupTable.h>
#include <vtkTransform.h>
#include <vtkMatrix4x4.h>

#include <cmath>

namespace {

constexpr double kTolerance = 1e-12;

bool areScalarsEqual(double lhs, double rhs)
{
    return std::abs(lhs - rhs) <= kTolerance;
}

bool areArraysEqual(const double* lhs, const double* rhs, int count)
{
    for (int i = 0; i < count; ++i) {
        if (!areScalarsEqual(lhs[i], rhs[i])) {
            return false;
        }
    }
    return true;
}

void copyArray(const double* source, double* target, int count)
{
    for (int i = 0; i < count; ++i) {
        target[i] = source[i];
    }
}

void deepCopyColumnMajorToVtkMatrix(vtkMatrix4x4* vtkMatrix, const double columnMajor[16])
{
    if (!vtkMatrix || !columnMajor) {
        return;
    }

    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            vtkMatrix->SetElement(row, column, columnMajor[column * 4 + row]);
        }
    }
}

}

ModelNodeDisplayManager::ModelNodeDisplayManager(SceneGraph* scene, const QString& windowId,
                                                   vtkRenderer* layer1, vtkRenderer* layer2,
                                                   vtkRenderer* layer3, QObject* parent)
    : NodeDisplayManager(scene, windowId, layer1, layer2, layer3, parent)
{
}

ModelNodeDisplayManager::~ModelNodeDisplayManager()
{
    clearAll();
}

bool ModelNodeDisplayManager::canHandleNode(NodeBase* node) const
{
    return dynamic_cast<ModelNode*>(node) != nullptr;
}

void ModelNodeDisplayManager::onNodeAdded(const QString& nodeId)
{
    if (m_entries.contains(nodeId)) {
        return;
    }
    buildEntry(nodeId);
}

void ModelNodeDisplayManager::onNodeRemoved(const QString& nodeId)
{
    removeEntry(nodeId);
}

void ModelNodeDisplayManager::onNodeModified(const QString& nodeId, NodeEventType eventType)
{
    if (!m_entries.contains(nodeId)) {
        buildEntry(nodeId);
        return;
    }

    switch (eventType) {
    case NodeEventType::ContentModified:
        updateContent(nodeId);
        break;
    case NodeEventType::DisplayChanged:
        updateDisplay(nodeId);
        break;
    case NodeEventType::TransformChanged:
        updateTransform(nodeId);
        break;
    default:
        updateContent(nodeId);
        updateDisplay(nodeId);
        break;
    }
}

void ModelNodeDisplayManager::reconcileWithScene()
{
    QVector<ModelNode*> sceneNodes = scene()->getAllModelNodes();
    QSet<QString> sceneIds;
    for (ModelNode* mn : sceneNodes) {
        sceneIds.insert(mn->getNodeId());
    }

    // Remove stale entries
    QStringList stale;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        if (!sceneIds.contains(it.key())) {
            stale.append(it.key());
        }
    }
    for (const QString& id : stale) {
        removeEntry(id);
    }

    // Add missing entries
    for (ModelNode* mn : sceneNodes) {
        const QString& id = mn->getNodeId();
        if (!m_entries.contains(id) && canHandleNode(mn)) {
            buildEntry(id);
        } else if (m_entries.contains(id)) {
            updateContent(id);
            updateDisplay(id);
            updateTransform(id);
        }
    }
}

void ModelNodeDisplayManager::clearAll()
{
    QStringList ids = m_entries.keys();
    for (const QString& id : ids) {
        removeEntry(id);
    }
}

void ModelNodeDisplayManager::buildEntry(const QString& nodeId)
{
    ModelNode* node = scene()->getNodeById<ModelNode>(nodeId);
    if (!node) {
        return;
    }

    int layer = getNodeLayerInWindow(node);

    vtkSmartPointer<vtkPolyData> polyData = node->getPolyData();
    if (!polyData) {
        polyData = vtkSmartPointer<vtkPolyData>::New();
    }

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(polyData);

    if (node->isUseScalarColor()) {
        mapper->ScalarVisibilityOn();
    } else {
        mapper->ScalarVisibilityOff();
    }

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->SetVisibility(0);

    ModelDisplayEntry entry;
    entry.actor = actor;
    entry.currentPolyData = polyData;
    entry.currentLayer = layer;

    vtkRenderer* renderer = getRenderer(layer);
    if (renderer) {
        renderer->AddActor(actor);
    }

    m_entries.insert(nodeId, entry);
    updateDisplay(nodeId);
    updateTransform(nodeId);
}

void ModelNodeDisplayManager::removeEntry(const QString& nodeId)
{
    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    vtkRenderer* renderer = getRenderer(it->currentLayer);
    if (renderer && it->actor) {
        renderer->RemoveActor(it->actor);
    }
    m_entries.erase(it);
}

void ModelNodeDisplayManager::updateContent(const QString& nodeId)
{
    ModelNode* node = scene()->getNodeById<ModelNode>(nodeId);
    if (!node) {
        return;
    }

    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    ModelDisplayEntry& entry = it.value();

    vtkSmartPointer<vtkPolyData> polyData = node->getPolyData();
    if (!polyData) {
        polyData = vtkSmartPointer<vtkPolyData>::New();
    }

    auto mapper = vtkPolyDataMapper::SafeDownCast(entry.actor->GetMapper());
    if (mapper && entry.currentPolyData != polyData) {
        mapper->SetInputData(polyData);
        mapper->Update();
        entry.currentPolyData = polyData;
    }
}

void ModelNodeDisplayManager::updateDisplay(const QString& nodeId)
{
    ModelNode* node = scene()->getNodeById<ModelNode>(nodeId);
    if (!node) {
        return;
    }

    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    ModelDisplayEntry& entry = it.value();

    bool visible = isNodeVisibleInWindow(node);
    int newLayer = getNodeLayerInWindow(node);

    if (entry.actorVisible != visible) {
        entry.actor->SetVisibility(visible ? 1 : 0);
        entry.actorVisible = visible;
    }

    applyVisualProperties(entry, node);

    auto mapper = vtkPolyDataMapper::SafeDownCast(entry.actor->GetMapper());
    if (mapper) {
        bool scalarVisibility = node->isUseScalarColor();
        if (!entry.hasScalarVisibility || entry.cachedScalarVisibility != scalarVisibility) {
            if (scalarVisibility) {
                mapper->ScalarVisibilityOn();
            } else {
                mapper->ScalarVisibilityOff();
            }
            entry.cachedScalarVisibility = scalarVisibility;
            entry.hasScalarVisibility = true;
        }
    }

    if (newLayer != entry.currentLayer) {
        vtkRenderer* oldRenderer = getRenderer(entry.currentLayer);
        vtkRenderer* newRenderer = getRenderer(newLayer);
        if (oldRenderer) {
            oldRenderer->RemoveActor(entry.actor);
        }
        if (newRenderer) {
            newRenderer->AddActor(entry.actor);
        }
        entry.currentLayer = newLayer;
    }
}

void ModelNodeDisplayManager::updateTransform(const QString& nodeId)
{
    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    ModelDisplayEntry& entry = it.value();

    double matrix[16];
    if (!scene()->getWorldTransformMatrix(nodeId, matrix)) {
        if (entry.hasWorldTransform) {
            entry.actor->SetUserTransform(nullptr);
            entry.hasWorldTransform = false;
        }
        return;
    }

    if (entry.hasWorldTransform && areArraysEqual(entry.cachedWorldMatrix, matrix, 16)) {
        return;
    }

    if (!entry.transform) {
        entry.transform = vtkSmartPointer<vtkTransform>::New();
        entry.transformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    }

    deepCopyColumnMajorToVtkMatrix(entry.transformMatrix, matrix);
    entry.transform->SetMatrix(entry.transformMatrix);
    entry.actor->SetUserTransform(entry.transform);
    copyArray(matrix, entry.cachedWorldMatrix, 16);
    entry.hasWorldTransform = true;
}

void ModelNodeDisplayManager::applyVisualProperties(ModelDisplayEntry& entry, ModelNode* node)
{
    vtkProperty* prop = entry.actor->GetProperty();

    QString renderMode = node->getRenderMode();
    if (!entry.hasRenderMode || entry.cachedRenderMode != renderMode) {
        if (renderMode == QStringLiteral("wireframe")) {
            prop->SetRepresentationToWireframe();
        } else if (renderMode == QStringLiteral("points")) {
            prop->SetRepresentationToPoints();
        } else {
            prop->SetRepresentationToSurface();
        }
        entry.cachedRenderMode = renderMode;
        entry.hasRenderMode = true;
    }

    double color[4];
    node->getColor(color);
    if (!entry.hasColor || !areArraysEqual(entry.cachedColor, color, 4)) {
        prop->SetColor(color[0], color[1], color[2]);
        copyArray(color, entry.cachedColor, 4);
        entry.hasColor = true;
    }

    double opacity = node->getOpacity();
    if (!entry.hasOpacity || !areScalarsEqual(entry.cachedOpacity, opacity)) {
        prop->SetOpacity(opacity);
        entry.cachedOpacity = opacity;
        entry.hasOpacity = true;
    }

    bool backfaceCulling = node->isBackfaceCulling();
    if (!entry.hasBackfaceCulling || entry.cachedBackfaceCulling != backfaceCulling) {
        prop->SetBackfaceCulling(backfaceCulling ? 1 : 0);
        entry.cachedBackfaceCulling = backfaceCulling;
        entry.hasBackfaceCulling = true;
    }

    const double materialAmbient = node->getMaterialAmbient();
    if (!entry.hasMaterialAmbient || !areScalarsEqual(entry.cachedMaterialAmbient, materialAmbient)) {
        prop->SetAmbient(materialAmbient);
        entry.cachedMaterialAmbient = materialAmbient;
        entry.hasMaterialAmbient = true;
    }

    const double materialDiffuse = node->getMaterialDiffuse();
    if (!entry.hasMaterialDiffuse || !areScalarsEqual(entry.cachedMaterialDiffuse, materialDiffuse)) {
        prop->SetDiffuse(materialDiffuse);
        entry.cachedMaterialDiffuse = materialDiffuse;
        entry.hasMaterialDiffuse = true;
    }

    const double materialSpecular = node->getMaterialSpecular();
    if (!entry.hasMaterialSpecular || !areScalarsEqual(entry.cachedMaterialSpecular, materialSpecular)) {
        prop->SetSpecular(materialSpecular);
        entry.cachedMaterialSpecular = materialSpecular;
        entry.hasMaterialSpecular = true;
    }

    const double materialSpecularPower = node->getMaterialSpecularPower();
    if (!entry.hasMaterialSpecularPower || !areScalarsEqual(entry.cachedMaterialSpecularPower, materialSpecularPower)) {
        prop->SetSpecularPower(materialSpecularPower);
        entry.cachedMaterialSpecularPower = materialSpecularPower;
        entry.hasMaterialSpecularPower = true;
    }

    const double materialRoughness = node->getMaterialRoughness();
    if (!entry.hasMaterialRoughness || !areScalarsEqual(entry.cachedMaterialRoughness, materialRoughness)) {
        prop->SetRoughness(materialRoughness);
        entry.cachedMaterialRoughness = materialRoughness;
        entry.hasMaterialRoughness = true;
    }

    bool previousShowEdges = entry.hasShowEdges ? entry.cachedShowEdges : false;
    bool showEdges = node->isShowEdges();
    if (!entry.hasShowEdges || previousShowEdges != showEdges) {
        prop->SetEdgeVisibility(showEdges ? 1 : 0);
        entry.cachedShowEdges = showEdges;
        entry.hasShowEdges = true;
    }

    if (showEdges) {
        double edgeColor[4];
        node->getEdgeColor(edgeColor);
        if (!entry.hasEdgeColor || !areArraysEqual(entry.cachedEdgeColor, edgeColor, 4) || !previousShowEdges) {
            prop->SetEdgeColor(edgeColor[0], edgeColor[1], edgeColor[2]);
            copyArray(edgeColor, entry.cachedEdgeColor, 4);
            entry.hasEdgeColor = true;
        }

        double edgeWidth = node->getEdgeWidth();
        if (!entry.hasEdgeWidth || !areScalarsEqual(entry.cachedEdgeWidth, edgeWidth) || !previousShowEdges) {
            prop->SetLineWidth(static_cast<float>(edgeWidth));
            entry.cachedEdgeWidth = edgeWidth;
            entry.hasEdgeWidth = true;
        }
    }
}
