#include "BillboardLineNodeDisplayManager.h"

#include "../logic/scene/nodes/BillboardLineNode.h"

#include <vtkProperty2D.h>
#include <vtkRenderer.h>

#include <QSet>
#include <QStringList>

#include <algorithm>
#include <array>
#include <cmath>

namespace {

using Vector3 = std::array<double, 3>;

constexpr double kTolerance = 1e-9;

Vector3 subtractVec3(const Vector3& lhs, const Vector3& rhs)
{
    return {lhs[0] - rhs[0], lhs[1] - rhs[1], lhs[2] - rhs[2]};
}

Vector3 addVec3(const Vector3& lhs, const Vector3& rhs)
{
    return {lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2]};
}

Vector3 scaleVec3(const Vector3& value, double scale)
{
    return {value[0] * scale, value[1] * scale, value[2] * scale};
}

double lengthVec3(const Vector3& value)
{
    return std::sqrt(value[0] * value[0] + value[1] * value[1] + value[2] * value[2]);
}

Vector3 transformPoint(const double matrix[16], const Vector3& point)
{
    return {
        matrix[0] * point[0] + matrix[4] * point[1] + matrix[8] * point[2] + matrix[12],
        matrix[1] * point[0] + matrix[5] * point[1] + matrix[9] * point[2] + matrix[13],
        matrix[2] * point[0] + matrix[6] * point[1] + matrix[10] * point[2] + matrix[14]
    };
}

}

BillboardLineNodeDisplayManager::BillboardLineNodeDisplayManager(
    SceneGraph* scene,
    const QString& windowId,
    vtkRenderer* layer1,
    vtkRenderer* layer2,
    vtkRenderer* layer3,
    QObject* parent)
    : NodeDisplayManager(scene, windowId, layer1, layer2, layer3, parent)
{
}

BillboardLineNodeDisplayManager::~BillboardLineNodeDisplayManager()
{
    clearAll();
}

bool BillboardLineNodeDisplayManager::canHandleNode(NodeBase* node) const
{
    return dynamic_cast<BillboardLineNode*>(node) != nullptr;
}

void BillboardLineNodeDisplayManager::onNodeAdded(const QString& nodeId)
{
    if (m_entries.contains(nodeId)) {
        return;
    }

    buildEntry(nodeId);
}

void BillboardLineNodeDisplayManager::onNodeRemoved(const QString& nodeId)
{
    removeEntry(nodeId);
}

void BillboardLineNodeDisplayManager::onNodeModified(const QString& nodeId, NodeEventType eventType)
{
    if (!m_entries.contains(nodeId)) {
        buildEntry(nodeId);
        return;
    }

    switch (eventType) {
    case NodeEventType::ContentModified:
    case NodeEventType::TransformChanged:
        updateContent(nodeId);
        break;
    case NodeEventType::DisplayChanged:
        updateContent(nodeId);
        updateDisplay(nodeId);
        break;
    default:
        updateContent(nodeId);
        updateDisplay(nodeId);
        break;
    }
}

void BillboardLineNodeDisplayManager::reconcileWithScene()
{
    const QVector<BillboardLineNode*> sceneNodes = scene()->getAllBillboardLineNodes();
    QSet<QString> sceneIds;
    for (BillboardLineNode* node : sceneNodes) {
        if (node) {
            sceneIds.insert(node->getNodeId());
        }
    }

    QStringList staleIds;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        if (!sceneIds.contains(it.key())) {
            staleIds.append(it.key());
        }
    }
    for (const QString& nodeId : staleIds) {
        removeEntry(nodeId);
    }

    for (BillboardLineNode* node : sceneNodes) {
        if (!node) {
            continue;
        }

        const QString nodeId = node->getNodeId();
        if (!m_entries.contains(nodeId)) {
            buildEntry(nodeId);
            continue;
        }

        updateContent(nodeId);
        updateDisplay(nodeId);
    }
}

void BillboardLineNodeDisplayManager::clearAll()
{
    const QStringList nodeIds = m_entries.keys();
    for (const QString& nodeId : nodeIds) {
        removeEntry(nodeId);
    }
}

void BillboardLineNodeDisplayManager::buildEntry(const QString& nodeId)
{
    BillboardLineNode* node = scene()->getNodeById<BillboardLineNode>(nodeId);
    if (!node) {
        return;
    }

    LineDisplayEntry entry;
    entry.currentLayer = getNodeLayerInWindow(node);
    entry.points = vtkSmartPointer<vtkPoints>::New();
    entry.cells = vtkSmartPointer<vtkCellArray>::New();
    entry.polyData = vtkSmartPointer<vtkPolyData>::New();
    entry.polyData->SetPoints(entry.points);
    entry.polyData->SetLines(entry.cells);
    entry.coordinate = vtkSmartPointer<vtkCoordinate>::New();
    entry.coordinate->SetCoordinateSystemToWorld();
    entry.mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
    entry.mapper->SetInputData(entry.polyData);
    entry.mapper->SetTransformCoordinate(entry.coordinate);
    entry.actor = vtkSmartPointer<vtkActor2D>::New();
    entry.actor->SetMapper(entry.mapper);
    entry.actor->SetVisibility(0);

    if (vtkRenderer* renderer = getRenderer(entry.currentLayer)) {
        renderer->AddActor2D(entry.actor);
    }

    m_entries.insert(nodeId, entry);
    updateContent(nodeId);
    updateDisplay(nodeId);
}

void BillboardLineNodeDisplayManager::removeEntry(const QString& nodeId)
{
    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    if (vtkRenderer* renderer = getRenderer(it->currentLayer)) {
        renderer->RemoveActor2D(it->actor);
    }
    m_entries.erase(it);
}

void BillboardLineNodeDisplayManager::updateContent(const QString& nodeId)
{
    BillboardLineNode* node = scene()->getNodeById<BillboardLineNode>(nodeId);
    auto it = m_entries.find(nodeId);
    if (!node || it == m_entries.end()) {
        return;
    }

    LineDisplayEntry& entry = it.value();
    entry.points->Reset();
    entry.cells->Reset();

    Vector3 startPoint = node->getStartPoint();
    Vector3 endPoint = node->getEndPoint();

    double worldMatrix[16];
    if (scene()->getWorldTransformMatrix(nodeId, worldMatrix)) {
        startPoint = transformPoint(worldMatrix, startPoint);
        endPoint = transformPoint(worldMatrix, endPoint);
    }

    const Vector3 delta = subtractVec3(endPoint, startPoint);
    const double totalLength = lengthVec3(delta);
    if (totalLength > kTolerance) {
        if (node->isDashed()) {
            const Vector3 direction = scaleVec3(delta, 1.0 / totalLength);
            const double dashLength = std::max(node->getDashLength(), 0.001);
            double cycleLength = dashLength + std::max(node->getGapLength(), 0.0);
            if (cycleLength <= kTolerance) {
                cycleLength = dashLength;
            }

            double cursor = 0.0;
            while (cursor < totalLength - kTolerance) {
                const double segmentStart = cursor;
                const double segmentEnd = std::min(cursor + dashLength, totalLength);
                if (segmentEnd > segmentStart) {
                    const Vector3 p0 = addVec3(startPoint, scaleVec3(direction, segmentStart));
                    const Vector3 p1 = addVec3(startPoint, scaleVec3(direction, segmentEnd));
                    const vtkIdType i0 = entry.points->InsertNextPoint(p0[0], p0[1], p0[2]);
                    const vtkIdType i1 = entry.points->InsertNextPoint(p1[0], p1[1], p1[2]);
                    vtkIdType ids[2] = {i0, i1};
                    entry.cells->InsertNextCell(2, ids);
                }
                cursor += cycleLength;
            }
        } else {
            const vtkIdType i0 = entry.points->InsertNextPoint(startPoint[0], startPoint[1], startPoint[2]);
            const vtkIdType i1 = entry.points->InsertNextPoint(endPoint[0], endPoint[1], endPoint[2]);
            vtkIdType ids[2] = {i0, i1};
            entry.cells->InsertNextCell(2, ids);
        }
    }

    entry.points->Modified();
    entry.cells->Modified();
    entry.polyData->Modified();
}

void BillboardLineNodeDisplayManager::updateDisplay(const QString& nodeId)
{
    BillboardLineNode* node = scene()->getNodeById<BillboardLineNode>(nodeId);
    auto it = m_entries.find(nodeId);
    if (!node || it == m_entries.end()) {
        return;
    }

    LineDisplayEntry& entry = it.value();
    vtkProperty2D* property = entry.actor->GetProperty();
    double color[4];
    node->getColor(color);
    property->SetColor(color[0], color[1], color[2]);
    property->SetOpacity(node->getOpacity());
    property->SetLineWidth(static_cast<float>(node->getLineWidth()));

    const bool visible = isNodeVisibleInWindow(node);
    entry.actor->SetVisibility(visible ? 1 : 0);

    const int newLayer = getNodeLayerInWindow(node);
    if (newLayer != entry.currentLayer) {
        if (vtkRenderer* oldRenderer = getRenderer(entry.currentLayer)) {
            oldRenderer->RemoveActor2D(entry.actor);
        }
        if (vtkRenderer* newRenderer = getRenderer(newLayer)) {
            newRenderer->AddActor2D(entry.actor);
        }
        entry.currentLayer = newLayer;
    }
}