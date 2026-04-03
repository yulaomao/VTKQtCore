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

constexpr double kDisplayStateEpsilon = 1e-9;

bool nearlyEqual(double left, double right)
{
    return std::fabs(left - right) <= kDisplayStateEpsilon;
}

template <std::size_t Size>
bool arrayEquals(const std::array<double, Size>& left,
                 const double (&right)[Size])
{
    for (std::size_t index = 0; index < Size; ++index) {
        if (!nearlyEqual(left[index], right[index])) {
            return false;
        }
    }

    return true;
}

template <std::size_t Size>
void copyArray(std::array<double, Size>& target,
               const double (&source)[Size])
{
    for (std::size_t index = 0; index < Size; ++index) {
        target[index] = source[index];
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

    bool visible = isNodeVisibleInWindow(node);
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
    actor->SetVisibility(visible ? 1 : 0);

    ModelDisplayEntry entry;
    entry.polyData = polyData;
    entry.mapper = mapper;
    entry.actor = actor;
    entry.transform = vtkSmartPointer<vtkTransform>::New();
    entry.transformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    entry.currentLayer = layer;
    entry.visible = visible;
    entry.scalarVisibility = node->isUseScalarColor();

    applyVisualProperties(entry, node);

    vtkRenderer* renderer = getRenderer(layer);
    if (renderer) {
        renderer->AddActor(actor);
    }

    m_entries.insert(nodeId, entry);
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

    vtkSmartPointer<vtkPolyData> polyData = node->getPolyData();
    if (!polyData) {
        polyData = vtkSmartPointer<vtkPolyData>::New();
    }

    if (it->mapper) {
        if (it->polyData != polyData) {
            it->mapper->SetInputData(polyData);
            it->polyData = polyData;
        } else {
            it->mapper->Modified();
        }
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

    bool visible = isNodeVisibleInWindow(node);
    int newLayer = getNodeLayerInWindow(node);

    if (it->visible != visible) {
        it->actor->SetVisibility(visible ? 1 : 0);
        it->visible = visible;
    }
    applyVisualProperties(*it, node);

    // Update scalar visibility
    if (it->mapper) {
        const bool useScalarColor = node->isUseScalarColor();
        if (it->scalarVisibility != useScalarColor) {
            if (useScalarColor) {
                it->mapper->ScalarVisibilityOn();
            } else {
                it->mapper->ScalarVisibilityOff();
            }
            it->scalarVisibility = useScalarColor;
        }
    }

    if (newLayer != it->currentLayer) {
        vtkRenderer* oldRenderer = getRenderer(it->currentLayer);
        vtkRenderer* newRenderer = getRenderer(newLayer);
        if (oldRenderer) {
            oldRenderer->RemoveActor(it->actor);
        }
        if (newRenderer) {
            newRenderer->AddActor(it->actor);
        }
        it->currentLayer = newLayer;
    }
}

void ModelNodeDisplayManager::updateTransform(const QString& nodeId)
{
    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    double matrix[16];
    if (!scene()->getWorldTransformMatrix(nodeId, matrix)) {
        if (it->hasTransform) {
            it->actor->SetUserTransform(nullptr);
            it->hasTransform = false;
        }
        return;
    }

    if (it->hasTransform && arrayEquals(it->transformValues, matrix)) {
        return;
    }

    if (!it->transform) {
        it->transform = vtkSmartPointer<vtkTransform>::New();
    }
    if (!it->transformMatrix) {
        it->transformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    }

    deepCopyColumnMajorToVtkMatrix(it->transformMatrix, matrix);
    it->transform->SetMatrix(it->transformMatrix);
    if (!it->hasTransform || it->actor->GetUserTransform() != it->transform) {
        it->actor->SetUserTransform(it->transform);
    }
    copyArray(it->transformValues, matrix);
    it->hasTransform = true;
}

void ModelNodeDisplayManager::applyVisualProperties(ModelDisplayEntry& entry, ModelNode* node)
{
    vtkProperty* prop = entry.actor->GetProperty();

    // Render mode
    QString renderMode = node->getRenderMode();
    if (entry.renderMode != renderMode) {
        if (renderMode == QStringLiteral("wireframe")) {
            prop->SetRepresentationToWireframe();
        } else if (renderMode == QStringLiteral("points")) {
            prop->SetRepresentationToPoints();
        } else {
            prop->SetRepresentationToSurface();
        }
        entry.renderMode = renderMode;
    }

    // Color and opacity
    double color[4];
    node->getColor(color);
    if (!arrayEquals(entry.color, color)) {
        prop->SetColor(color[0], color[1], color[2]);
        copyArray(entry.color, color);
    }
    const double opacity = node->getOpacity();
    if (!nearlyEqual(entry.opacity, opacity)) {
        prop->SetOpacity(opacity);
        entry.opacity = opacity;
    }

    // Backface culling
    const bool backfaceCulling = node->isBackfaceCulling();
    if (entry.backfaceCulling != backfaceCulling) {
        prop->SetBackfaceCulling(backfaceCulling ? 1 : 0);
        entry.backfaceCulling = backfaceCulling;
    }

    // Edge visibility
    const bool showEdges = node->isShowEdges();
    if (entry.edgeVisibility != showEdges) {
        prop->SetEdgeVisibility(showEdges ? 1 : 0);
        entry.edgeVisibility = showEdges;
    }
    if (showEdges) {
        double edgeColor[4];
        node->getEdgeColor(edgeColor);
        if (!arrayEquals(entry.edgeColor, edgeColor)) {
            prop->SetEdgeColor(edgeColor[0], edgeColor[1], edgeColor[2]);
            copyArray(entry.edgeColor, edgeColor);
        }
        const double edgeWidth = node->getEdgeWidth();
        if (!nearlyEqual(entry.edgeWidth, edgeWidth)) {
            prop->SetLineWidth(static_cast<float>(edgeWidth));
            entry.edgeWidth = edgeWidth;
        }
    }
}
