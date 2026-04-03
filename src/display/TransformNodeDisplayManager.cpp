#include "TransformNodeDisplayManager.h"
#include "../logic/scene/nodes/TransformNode.h"

#include <vtkLineSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkTransform.h>

#include <cmath>

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

static vtkSmartPointer<vtkActor> createAxisActor(const double origin[3],
                                                   const double endpoint[3],
                                                   const double color[4])
{
    auto lineSource = vtkSmartPointer<vtkLineSource>::New();
    lineSource->SetPoint1(origin[0], origin[1], origin[2]);
    lineSource->SetPoint2(endpoint[0], endpoint[1], endpoint[2]);
    lineSource->Update();

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(lineSource->GetOutputPort());

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(color[0], color[1], color[2]);
    actor->GetProperty()->SetOpacity(color[3]);
    actor->GetProperty()->SetLineWidth(2.0f);

    return actor;
}

static void updateAxisActor(vtkActor* actor, const double origin[3],
                             const double endpoint[3], const double color[4])
{
    auto lineSource = vtkSmartPointer<vtkLineSource>::New();
    lineSource->SetPoint1(origin[0], origin[1], origin[2]);
    lineSource->SetPoint2(endpoint[0], endpoint[1], endpoint[2]);
    lineSource->Update();

    auto mapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    mapper->SetInputConnection(lineSource->GetOutputPort());

    actor->SetMapper(mapper);
    actor->GetProperty()->SetColor(color[0], color[1], color[2]);
    actor->GetProperty()->SetOpacity(color[3]);
}

void TransformNodeDisplayManager::buildEntry(const QString& nodeId)
{
    TransformNode* node = scene()->getNodeById<TransformNode>(nodeId);
    if (!node) {
        return;
    }

    bool visible = isNodeVisibleInWindow(node) && node->isShowAxes();
    int layer = getNodeLayerInWindow(node);

    double matrix[16];
    if (!scene()->getWorldTransformMatrix(nodeId, matrix)) {
        node->getMatrixTransformToParent(matrix);
    }
    double origin[3] = {matrix[12], matrix[13], matrix[14]};
    double axesLength = node->getAxesLength();

    // Compute endpoints for each axis
    double xEnd[3], yEnd[3], zEnd[3];
    computeAxisEndpoint(matrix, 0, axesLength, xEnd);
    computeAxisEndpoint(matrix, 1, axesLength, yEnd);
    computeAxisEndpoint(matrix, 2, axesLength, zEnd);

    double xColor[4], yColor[4], zColor[4];
    node->getAxesColorX(xColor);
    node->getAxesColorY(yColor);
    node->getAxesColorZ(zColor);

    auto xActor = createAxisActor(origin, xEnd, xColor);
    auto yActor = createAxisActor(origin, yEnd, yColor);
    auto zActor = createAxisActor(origin, zEnd, zColor);

    xActor->SetVisibility(visible ? 1 : 0);
    yActor->SetVisibility(visible ? 1 : 0);
    zActor->SetVisibility(visible ? 1 : 0);

    vtkRenderer* renderer = getRenderer(layer);
    if (renderer) {
        renderer->AddActor(xActor);
        renderer->AddActor(yActor);
        renderer->AddActor(zActor);
    }

    TransformDisplayEntry entry;
    entry.xAxisActor = xActor;
    entry.yAxisActor = yActor;
    entry.zAxisActor = zActor;
    entry.currentLayer = layer;
    m_entries.insert(nodeId, entry);
}

void TransformNodeDisplayManager::removeEntry(const QString& nodeId)
{
    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    vtkRenderer* renderer = getRenderer(it->currentLayer);
    if (renderer) {
        if (it->xAxisActor) {
            renderer->RemoveActor(it->xAxisActor);
        }
        if (it->yAxisActor) {
            renderer->RemoveActor(it->yAxisActor);
        }
        if (it->zAxisActor) {
            renderer->RemoveActor(it->zAxisActor);
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

    double xColor[4], yColor[4], zColor[4];
    node->getAxesColorX(xColor);
    node->getAxesColorY(yColor);
    node->getAxesColorZ(zColor);

    updateAxisActor(it->xAxisActor, origin, xEnd, xColor);
    updateAxisActor(it->yAxisActor, origin, yEnd, yColor);
    updateAxisActor(it->zAxisActor, origin, zEnd, zColor);
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

    bool visible = isNodeVisibleInWindow(node) && node->isShowAxes();
    int newLayer = getNodeLayerInWindow(node);

    it->xAxisActor->SetVisibility(visible ? 1 : 0);
    it->yAxisActor->SetVisibility(visible ? 1 : 0);
    it->zAxisActor->SetVisibility(visible ? 1 : 0);

    // Update colors
    double xColor[4], yColor[4], zColor[4];
    node->getAxesColorX(xColor);
    node->getAxesColorY(yColor);
    node->getAxesColorZ(zColor);

    it->xAxisActor->GetProperty()->SetColor(xColor[0], xColor[1], xColor[2]);
    it->yAxisActor->GetProperty()->SetColor(yColor[0], yColor[1], yColor[2]);
    it->zAxisActor->GetProperty()->SetColor(zColor[0], zColor[1], zColor[2]);

    if (newLayer != it->currentLayer) {
        vtkRenderer* oldRenderer = getRenderer(it->currentLayer);
        vtkRenderer* newRenderer = getRenderer(newLayer);
        if (oldRenderer) {
            oldRenderer->RemoveActor(it->xAxisActor);
            oldRenderer->RemoveActor(it->yAxisActor);
            oldRenderer->RemoveActor(it->zAxisActor);
        }
        if (newRenderer) {
            newRenderer->AddActor(it->xAxisActor);
            newRenderer->AddActor(it->yAxisActor);
            newRenderer->AddActor(it->zAxisActor);
        }
        it->currentLayer = newLayer;
    }
}
