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

#include <array>
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

void LineNodeDisplayManager::populatePolyLineData(LineNode* node,
                                                  vtkPoints* points,
                                                  vtkCellArray* cells,
                                                  vtkPolyData* polyData) const
{
    if (!node || !points || !cells || !polyData) {
        return;
    }

    points->Reset();
    cells->Reset();

    int vertexCount = node->getVertexCount();

    for (int i = 0; i < vertexCount; ++i) {
        std::array<double, 3> v = node->getVertex(i);
        points->InsertNextPoint(v[0], v[1], v[2]);
    }

    if (vertexCount >= 2) {
        int segmentCount = node->isClosed() ? vertexCount : vertexCount - 1;
        for (int i = 0; i < segmentCount; ++i) {
            vtkIdType ids[2] = {i, (i + 1) % vertexCount};
            cells->InsertNextCell(2, ids);
        }
    }

    polyData->SetPoints(points);
    polyData->SetLines(cells);
    points->Modified();
    cells->Modified();
    polyData->Modified();
}

void LineNodeDisplayManager::buildEntry(const QString& nodeId)
{
    LineNode* node = scene()->getNodeById<LineNode>(nodeId);
    if (!node) {
        return;
    }

    bool visible = isNodeVisibleInWindow(node);
    int layer = getNodeLayerInWindow(node);

    LineDisplayEntry entry;
    entry.points = vtkSmartPointer<vtkPoints>::New();
    entry.cells = vtkSmartPointer<vtkCellArray>::New();
    entry.polyData = vtkSmartPointer<vtkPolyData>::New();
    populatePolyLineData(node, entry.points, entry.cells, entry.polyData);

    entry.mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    entry.mapper->SetInputData(entry.polyData);

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(entry.mapper);

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

    entry.actor = actor;
    entry.transform = vtkSmartPointer<vtkTransform>::New();
    entry.transformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    copyArray(entry.color, color);
    entry.opacity = node->getOpacity();
    entry.lineWidth = node->getLineWidth();
    entry.dashed = node->isDashed();
    entry.visible = visible;
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

    populatePolyLineData(node, it->points, it->cells, it->polyData);
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
    const double opacity = node->getOpacity();
    const double lineWidth = node->getLineWidth();
    const bool dashed = node->isDashed();

    if (!arrayEquals(it->color, color)) {
        it->actor->GetProperty()->SetColor(color[0], color[1], color[2]);
        copyArray(it->color, color);
    }
    if (!nearlyEqual(it->opacity, opacity)) {
        it->actor->GetProperty()->SetOpacity(opacity);
        it->opacity = opacity;
    }
    if (!nearlyEqual(it->lineWidth, lineWidth)) {
        it->actor->GetProperty()->SetLineWidth(static_cast<float>(lineWidth));
        it->lineWidth = lineWidth;
    }

    if (it->dashed != dashed) {
        if (dashed) {
            it->actor->GetProperty()->SetLineStipplePattern(0xFF00);
            it->actor->GetProperty()->SetLineStippleRepeatFactor(1);
        } else {
            it->actor->GetProperty()->SetLineStipplePattern(0xFFFF);
        }
        it->dashed = dashed;
    }

    if (it->visible != visible) {
        it->actor->SetVisibility(visible ? 1 : 0);
        it->visible = visible;
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

void LineNodeDisplayManager::updateTransform(const QString& nodeId)
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
