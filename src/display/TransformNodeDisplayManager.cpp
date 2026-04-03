#include "TransformNodeDisplayManager.h"
#include "../logic/scene/nodes/TransformNode.h"

#include <vtkLineSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>

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

}

TransformNodeDisplayManager::TransformNodeDisplayManager(SceneGraph* scene,
                                                         const QString& windowId,
                                                         vtkRenderer* layer1,
                                                         vtkRenderer* layer2,
                                                         vtkRenderer* layer3,
                                                         QObject* parent)
    : NodeDisplayManager(scene, windowId, layer1, layer2, layer3, parent)
{
}

TransformNodeDisplayManager::~TransformNodeDisplayManager()
{
    clearAll();
}

bool TransformNodeDisplayManager::canHandleNode(NodeBase* node) const
{
    return dynamic_cast<TransformNode*>(node) != nullptr;
}

void TransformNodeDisplayManager::onNodeAdded(const QString& nodeId)
{
    if (m_entries.contains(nodeId)) {
        return;
    }
    buildEntry(nodeId);
}

void TransformNodeDisplayManager::onNodeRemoved(const QString& nodeId)
{
    removeEntry(nodeId);
}

void TransformNodeDisplayManager::onNodeModified(const QString& nodeId, NodeEventType eventType)
{
    if (!m_entries.contains(nodeId)) {
        buildEntry(nodeId);
        return;
    }

    switch (eventType) {
    case NodeEventType::ContentModified:
    case NodeEventType::TransformChanged:
        updateAxes(nodeId);
        break;
    case NodeEventType::DisplayChanged:
        updateAxes(nodeId);
        updateDisplay(nodeId);
        break;
    default:
        updateAxes(nodeId);
        updateDisplay(nodeId);
        break;
    }
}

void TransformNodeDisplayManager::reconcileWithScene()
{
    QVector<TransformNode*> sceneNodes = scene()->getAllTransformNodes();
    QSet<QString> sceneIds;
    for (TransformNode* tn : sceneNodes) {
        sceneIds.insert(tn->getNodeId());
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
    for (TransformNode* tn : sceneNodes) {
        const QString& id = tn->getNodeId();
        if (!m_entries.contains(id) && canHandleNode(tn)) {
            buildEntry(id);
        } else if (m_entries.contains(id)) {
            updateAxes(id);
            updateDisplay(id);
        }
    }
}

void TransformNodeDisplayManager::clearAll()
{
    QStringList ids = m_entries.keys();
    for (const QString& id : ids) {
        removeEntry(id);
    }
}

void TransformNodeDisplayManager::computeAxisEndpoint(const double matrix[16], int axisIndex,
                                                      double length, double out[3]) const
{
    double origin[3] = {matrix[12], matrix[13], matrix[14]};
    double direction[3] = {
        matrix[axisIndex * 4 + 0],
        matrix[axisIndex * 4 + 1],
        matrix[axisIndex * 4 + 2]
    };

    // Normalize direction
    double mag = std::sqrt(direction[0] * direction[0] +
                           direction[1] * direction[1] +
                           direction[2] * direction[2]);
    if (mag > 1e-12) {
        direction[0] /= mag;
        direction[1] /= mag;
        direction[2] /= mag;
    }

    out[0] = origin[0] + direction[0] * length;
    out[1] = origin[1] + direction[1] * length;
    out[2] = origin[2] + direction[2] * length;
}

void TransformNodeDisplayManager::updateAxisGeometry(AxisDisplayEntry& axis,
                                                     const double origin[3],
                                                     const double endpoint[3])
{
    if (axis.hasGeometry &&
        areArraysEqual(axis.cachedOrigin, origin, 3) &&
        areArraysEqual(axis.cachedEndpoint, endpoint, 3)) {
        return;
    }

    axis.lineSource->SetPoint1(origin[0], origin[1], origin[2]);
    axis.lineSource->SetPoint2(endpoint[0], endpoint[1], endpoint[2]);
    copyArray(origin, axis.cachedOrigin, 3);
    copyArray(endpoint, axis.cachedEndpoint, 3);
    axis.hasGeometry = true;
}

void TransformNodeDisplayManager::updateAxisColor(AxisDisplayEntry& axis, const double color[4])
{
    if (axis.hasColor && areArraysEqual(axis.cachedColor, color, 4)) {
        return;
    }

    axis.actor->GetProperty()->SetColor(color[0], color[1], color[2]);
    axis.actor->GetProperty()->SetOpacity(color[3]);
    copyArray(color, axis.cachedColor, 4);
    axis.hasColor = true;
}

void TransformNodeDisplayManager::buildEntry(const QString& nodeId)
{
    TransformNode* node = scene()->getNodeById<TransformNode>(nodeId);
    if (!node) {
        return;
    }

    int layer = getNodeLayerInWindow(node);

    TransformDisplayEntry entry;
    entry.currentLayer = layer;

    auto initializeAxis = [](AxisDisplayEntry& axis) {
        axis.lineSource = vtkSmartPointer<vtkLineSource>::New();
        axis.mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
        axis.mapper->SetInputConnection(axis.lineSource->GetOutputPort());
        axis.actor = vtkSmartPointer<vtkActor>::New();
        axis.actor->SetMapper(axis.mapper);
        axis.actor->GetProperty()->SetLineWidth(2.0f);
        axis.actor->SetVisibility(0);
    };

    initializeAxis(entry.xAxis);
    initializeAxis(entry.yAxis);
    initializeAxis(entry.zAxis);

    vtkRenderer* renderer = getRenderer(layer);
    if (renderer) {
        renderer->AddActor(entry.xAxis.actor);
        renderer->AddActor(entry.yAxis.actor);
        renderer->AddActor(entry.zAxis.actor);
    }

    m_entries.insert(nodeId, entry);
    updateDisplay(nodeId);
}

void TransformNodeDisplayManager::removeEntry(const QString& nodeId)
{
    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    vtkRenderer* renderer = getRenderer(it->currentLayer);
    if (renderer) {
        if (it->xAxis.actor) {
            renderer->RemoveActor(it->xAxis.actor);
        }
        if (it->yAxis.actor) {
            renderer->RemoveActor(it->yAxis.actor);
        }
        if (it->zAxis.actor) {
            renderer->RemoveActor(it->zAxis.actor);
        }
    }
    m_entries.erase(it);
}

void TransformNodeDisplayManager::updateAxes(const QString& nodeId)
{
    TransformNode* node = scene()->getNodeById<TransformNode>(nodeId);
    if (!node) {
        return;
    }

    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    TransformDisplayEntry& entry = it.value();
    bool visible = isNodeVisibleInWindow(node) && node->isShowAxes();
    if (!visible) {
        entry.geometryDirty = true;
        return;
    }

    double matrix[16];
    if (!scene()->getWorldTransformMatrix(nodeId, matrix)) {
        node->getMatrixTransformToParent(matrix);
    }
    double origin[3] = {matrix[12], matrix[13], matrix[14]};
    double axesLength = node->getAxesLength();

    double xEnd[3], yEnd[3], zEnd[3];
    computeAxisEndpoint(matrix, 0, axesLength, xEnd);
    computeAxisEndpoint(matrix, 1, axesLength, yEnd);
    computeAxisEndpoint(matrix, 2, axesLength, zEnd);

    updateAxisGeometry(entry.xAxis, origin, xEnd);
    updateAxisGeometry(entry.yAxis, origin, yEnd);
    updateAxisGeometry(entry.zAxis, origin, zEnd);
    entry.geometryDirty = false;
}

void TransformNodeDisplayManager::updateDisplay(const QString& nodeId)
{
    TransformNode* node = scene()->getNodeById<TransformNode>(nodeId);
    if (!node) {
        return;
    }

    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    TransformDisplayEntry& entry = it.value();

    bool visible = isNodeVisibleInWindow(node) && node->isShowAxes();
    int newLayer = getNodeLayerInWindow(node);

    if (visible && (!entry.visible || entry.geometryDirty)) {
        updateAxes(nodeId);
    }

    if (entry.visible != visible) {
        entry.xAxis.actor->SetVisibility(visible ? 1 : 0);
        entry.yAxis.actor->SetVisibility(visible ? 1 : 0);
        entry.zAxis.actor->SetVisibility(visible ? 1 : 0);
        entry.visible = visible;
    }

    double xColor[4], yColor[4], zColor[4];
    node->getAxesColorX(xColor);
    node->getAxesColorY(yColor);
    node->getAxesColorZ(zColor);

    updateAxisColor(entry.xAxis, xColor);
    updateAxisColor(entry.yAxis, yColor);
    updateAxisColor(entry.zAxis, zColor);

    if (newLayer != entry.currentLayer) {
        vtkRenderer* oldRenderer = getRenderer(entry.currentLayer);
        vtkRenderer* newRenderer = getRenderer(newLayer);
        if (oldRenderer) {
            oldRenderer->RemoveActor(entry.xAxis.actor);
            oldRenderer->RemoveActor(entry.yAxis.actor);
            oldRenderer->RemoveActor(entry.zAxis.actor);
        }
        if (newRenderer) {
            newRenderer->AddActor(entry.xAxis.actor);
            newRenderer->AddActor(entry.yAxis.actor);
            newRenderer->AddActor(entry.zAxis.actor);
        }
        entry.currentLayer = newLayer;
    }
}
