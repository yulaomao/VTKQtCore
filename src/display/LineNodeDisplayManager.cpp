#include "LineNodeDisplayManager.h"
#include "../logic/scene/nodes/LineNode.h"

#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkTransform.h>
#include <vtkMatrix4x4.h>

LineNodeDisplayManager::LineNodeDisplayManager(SceneGraph* scene, const QString& windowId,
                                                 vtkRenderer* layer1, vtkRenderer* layer2,
                                                 vtkRenderer* layer3, QObject* parent)
    : NodeDisplayManager(scene, windowId, layer1, layer2, layer3, parent)
{
}

LineNodeDisplayManager::~LineNodeDisplayManager()
{
    clearAll();
}

bool LineNodeDisplayManager::canHandleNode(NodeBase* node) const
{
    return dynamic_cast<LineNode*>(node) != nullptr;
}

void LineNodeDisplayManager::onNodeAdded(const QString& nodeId)
{
    if (m_entries.contains(nodeId)) {
        return;
    }
    buildEntry(nodeId);
}

void LineNodeDisplayManager::onNodeRemoved(const QString& nodeId)
{
    removeEntry(nodeId);
}

void LineNodeDisplayManager::onNodeModified(const QString& nodeId, NodeEventType eventType)
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

void LineNodeDisplayManager::reconcileWithScene()
{
    QVector<LineNode*> sceneNodes = scene()->getAllLineNodes();
    QSet<QString> sceneIds;
    for (LineNode* ln : sceneNodes) {
        sceneIds.insert(ln->getNodeId());
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
    for (LineNode* ln : sceneNodes) {
        const QString& id = ln->getNodeId();
        if (!m_entries.contains(id) && canHandleNode(ln)) {
            buildEntry(id);
        } else if (m_entries.contains(id)) {
            updateContent(id);
            updateDisplay(id);
            updateTransform(id);
        }
    }
}

void LineNodeDisplayManager::clearAll()
{
    QStringList ids = m_entries.keys();
    for (const QString& id : ids) {
        removeEntry(id);
    }
}

vtkSmartPointer<vtkPolyData> LineNodeDisplayManager::buildPolyLine(LineNode* node) const
{
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto cells = vtkSmartPointer<vtkCellArray>::New();

    int vertexCount = node->getVertexCount();
    if (vertexCount < 2) {
        auto polyData = vtkSmartPointer<vtkPolyData>::New();
        polyData->SetPoints(points);
        polyData->SetLines(cells);
        return polyData;
    }

    for (int i = 0; i < vertexCount; ++i) {
        std::array<double, 3> v = node->getVertex(i);
        points->InsertNextPoint(v[0], v[1], v[2]);
    }

    int segmentCount = node->isClosed() ? vertexCount : vertexCount - 1;
    for (int i = 0; i < segmentCount; ++i) {
        vtkIdType ids[2] = {i, (i + 1) % vertexCount};
        cells->InsertNextCell(2, ids);
    }

    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetLines(cells);
    return polyData;
}

void LineNodeDisplayManager::buildEntry(const QString& nodeId)
{
    LineNode* node = scene()->getNodeById<LineNode>(nodeId);
    if (!node) {
        return;
    }

    bool visible = isNodeVisibleInWindow(node);
    int layer = getNodeLayerInWindow(node);

    vtkSmartPointer<vtkPolyData> polyData = buildPolyLine(node);

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(polyData);

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);

    double color[4];
    node->getColor(color);
    actor->GetProperty()->SetColor(color[0], color[1], color[2]);
    actor->GetProperty()->SetOpacity(node->getOpacity());
    actor->GetProperty()->SetLineWidth(static_cast<float>(node->getLineWidth()));

    if (node->isDashed()) {
        actor->GetProperty()->SetLineStipplePattern(0xFF00);
        actor->GetProperty()->SetLineStippleRepeatFactor(1);
    }

    actor->SetVisibility(visible ? 1 : 0);

    vtkRenderer* renderer = getRenderer(layer);
    if (renderer) {
        renderer->AddActor(actor);
    }

    LineDisplayEntry entry;
    entry.actor = actor;
    entry.currentLayer = layer;
    m_entries.insert(nodeId, entry);
}

void LineNodeDisplayManager::removeEntry(const QString& nodeId)
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

void LineNodeDisplayManager::updateContent(const QString& nodeId)
{
    LineNode* node = scene()->getNodeById<LineNode>(nodeId);
    if (!node) {
        return;
    }

    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    vtkSmartPointer<vtkPolyData> polyData = buildPolyLine(node);

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputData(polyData);
    it->actor->SetMapper(mapper);
}

void LineNodeDisplayManager::updateDisplay(const QString& nodeId)
{
    LineNode* node = scene()->getNodeById<LineNode>(nodeId);
    if (!node) {
        return;
    }

    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    bool visible = isNodeVisibleInWindow(node);
    int newLayer = getNodeLayerInWindow(node);

    double color[4];
    node->getColor(color);
    it->actor->GetProperty()->SetColor(color[0], color[1], color[2]);
    it->actor->GetProperty()->SetOpacity(node->getOpacity());
    it->actor->GetProperty()->SetLineWidth(static_cast<float>(node->getLineWidth()));

    if (node->isDashed()) {
        it->actor->GetProperty()->SetLineStipplePattern(0xFF00);
        it->actor->GetProperty()->SetLineStippleRepeatFactor(1);
    } else {
        it->actor->GetProperty()->SetLineStipplePattern(0xFFFF);
    }

    it->actor->SetVisibility(visible ? 1 : 0);

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

void LineNodeDisplayManager::updateTransform(const QString& nodeId)
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
