#include "ModelNodeDisplayManager.h"
#include "../logic/scene/nodes/ModelNode.h"

#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkLookupTable.h>
#include <vtkTransform.h>
#include <vtkMatrix4x4.h>

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
    entry.actor = actor;
    entry.currentLayer = layer;

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

    auto mapper = vtkPolyDataMapper::SafeDownCast(it->actor->GetMapper());
    if (mapper) {
        mapper->SetInputData(polyData);
        mapper->Update();
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

    it->actor->SetVisibility(visible ? 1 : 0);
    applyVisualProperties(*it, node);

    // Update scalar visibility
    auto mapper = vtkPolyDataMapper::SafeDownCast(it->actor->GetMapper());
    if (mapper) {
        if (node->isUseScalarColor()) {
            mapper->ScalarVisibilityOn();
        } else {
            mapper->ScalarVisibilityOff();
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
        it->actor->SetUserTransform(nullptr);
        return;
    }

    auto transform = vtkSmartPointer<vtkTransform>::New();
    auto mat = vtkSmartPointer<vtkMatrix4x4>::New();
    mat->DeepCopy(matrix);
    transform->SetMatrix(mat);

    it->actor->SetUserTransform(transform);
}

void ModelNodeDisplayManager::applyVisualProperties(ModelDisplayEntry& entry, ModelNode* node)
{
    vtkProperty* prop = entry.actor->GetProperty();

    // Render mode
    QString renderMode = node->getRenderMode();
    if (renderMode == QStringLiteral("wireframe")) {
        prop->SetRepresentationToWireframe();
    } else if (renderMode == QStringLiteral("points")) {
        prop->SetRepresentationToPoints();
    } else {
        prop->SetRepresentationToSurface();
    }

    // Color and opacity
    double color[4];
    node->getColor(color);
    prop->SetColor(color[0], color[1], color[2]);
    prop->SetOpacity(node->getOpacity());

    // Backface culling
    prop->SetBackfaceCulling(node->isBackfaceCulling() ? 1 : 0);

    // Edge visibility
    prop->SetEdgeVisibility(node->isShowEdges() ? 1 : 0);
    if (node->isShowEdges()) {
        double edgeColor[4];
        node->getEdgeColor(edgeColor);
        prop->SetEdgeColor(edgeColor[0], edgeColor[1], edgeColor[2]);
        prop->SetLineWidth(static_cast<float>(node->getEdgeWidth()));
    }
}
