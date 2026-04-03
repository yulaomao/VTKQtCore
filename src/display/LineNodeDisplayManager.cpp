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

void LineNodeDisplayManager::buildEntry(const QString& nodeId)
{
    LineNode* node = scene()->getNodeById<LineNode>(nodeId);
    if (!node) {
        return;
    }

    int layer = getNodeLayerInWindow(node);

    LineDisplayEntry entry;
    entry.currentLayer = layer;
    entry.points = vtkSmartPointer<vtkPoints>::New();
    entry.cells = vtkSmartPointer<vtkCellArray>::New();
    entry.polyData = vtkSmartPointer<vtkPolyData>::New();
    entry.polyData->SetPoints(entry.points);
    entry.polyData->SetLines(entry.cells);
    entry.mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    entry.mapper->SetInputData(entry.polyData);
    entry.actor = vtkSmartPointer<vtkActor>::New();
    entry.actor->SetMapper(entry.mapper);
    entry.actor->SetVisibility(0);

    vtkRenderer* renderer = getRenderer(layer);
    if (renderer) {
        renderer->AddActor(entry.actor);
    }

    m_entries.insert(nodeId, entry);
    updateContent(nodeId);
    updateDisplay(nodeId);
    updateTransform(nodeId);
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

    LineDisplayEntry& entry = it.value();

    entry.points->Reset();
    entry.cells->Reset();

    int vertexCount = node->getVertexCount();
    for (int i = 0; i < vertexCount; ++i) {
        std::array<double, 3> vertex = node->getVertex(i);
        entry.points->InsertNextPoint(vertex[0], vertex[1], vertex[2]);
    }

    if (vertexCount >= 2) {
        int segmentCount = node->isClosed() ? vertexCount : vertexCount - 1;
        for (int i = 0; i < segmentCount; ++i) {
            vtkIdType ids[2] = {
                static_cast<vtkIdType>(i),
                static_cast<vtkIdType>((i + 1) % vertexCount)
            };
            entry.cells->InsertNextCell(2, ids);
        }
    }

    entry.points->Modified();
    entry.cells->Modified();
    entry.polyData->Modified();
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

    LineDisplayEntry& entry = it.value();

    bool visible = isNodeVisibleInWindow(node);
    int newLayer = getNodeLayerInWindow(node);

    double color[4];
    node->getColor(color);
    if (!entry.hasColor || !areArraysEqual(entry.cachedColor, color, 4)) {
        entry.actor->GetProperty()->SetColor(color[0], color[1], color[2]);
        copyArray(color, entry.cachedColor, 4);
        entry.hasColor = true;
    }

    double opacity = node->getOpacity();
    if (!entry.hasOpacity || !areScalarsEqual(entry.cachedOpacity, opacity)) {
        entry.actor->GetProperty()->SetOpacity(opacity);
        entry.cachedOpacity = opacity;
        entry.hasOpacity = true;
    }

    double lineWidth = node->getLineWidth();
    if (!entry.hasLineWidth || !areScalarsEqual(entry.cachedLineWidth, lineWidth)) {
        entry.actor->GetProperty()->SetLineWidth(static_cast<float>(lineWidth));
        entry.cachedLineWidth = lineWidth;
        entry.hasLineWidth = true;
    }

    bool dashed = node->isDashed();
    if (entry.dashed != dashed) {
        if (dashed) {
            entry.actor->GetProperty()->SetLineStipplePattern(0xFF00);
            entry.actor->GetProperty()->SetLineStippleRepeatFactor(1);
        } else {
            entry.actor->GetProperty()->SetLineStipplePattern(0xFFFF);
        }
        entry.dashed = dashed;
    }

    if (entry.actorVisible != visible) {
        entry.actor->SetVisibility(visible ? 1 : 0);
        entry.actorVisible = visible;
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

void LineNodeDisplayManager::updateTransform(const QString& nodeId)
{
    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    LineDisplayEntry& entry = it.value();

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
