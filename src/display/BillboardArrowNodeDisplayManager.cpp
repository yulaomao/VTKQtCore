#include "BillboardArrowNodeDisplayManager.h"

#include "../logic/scene/nodes/BillboardArrowNode.h"

#include <vtkCamera.h>
#include <vtkCommand.h>
#include <vtkProperty2D.h>
#include <vtkRenderer.h>
#include <vtkTriangle.h>

#include <QSet>
#include <QStringList>

#include <array>
#include <cmath>

namespace {

using Vector3 = std::array<double, 3>;

constexpr double kOrientationProbeLength = 1.0;
constexpr double kLengthTolerance = 1e-9;

double lengthVec3(const Vector3& vector)
{
    return std::sqrt(vector[0] * vector[0] + vector[1] * vector[1] + vector[2] * vector[2]);
}

Vector3 normalizeVec3(const Vector3& vector, const Vector3& fallback)
{
    const double length = lengthVec3(vector);
    if (length <= kLengthTolerance) {
        return fallback;
    }

    const double inverse = 1.0 / length;
    return {vector[0] * inverse, vector[1] * inverse, vector[2] * inverse};
}

Vector3 scaleVec3(const Vector3& vector, double scale)
{
    return {vector[0] * scale, vector[1] * scale, vector[2] * scale};
}

Vector3 addVec3(const Vector3& lhs, const Vector3& rhs)
{
    return {lhs[0] + rhs[0], lhs[1] + rhs[1], lhs[2] + rhs[2]};
}

Vector3 transformPoint(const double matrix[16], const Vector3& point)
{
    return {
        matrix[0] * point[0] + matrix[4] * point[1] + matrix[8] * point[2] + matrix[12],
        matrix[1] * point[0] + matrix[5] * point[1] + matrix[9] * point[2] + matrix[13],
        matrix[2] * point[0] + matrix[6] * point[1] + matrix[10] * point[2] + matrix[14]
    };
}

std::array<double, 2> defaultDisplayDirection(const QString& direction)
{
    return direction == QStringLiteral("up")
        ? std::array<double, 2>{0.0, -1.0}
        : std::array<double, 2>{0.0, 1.0};
}

}

BillboardArrowNodeDisplayManager::BillboardArrowNodeDisplayManager(
    SceneGraph* scene,
    const QString& windowId,
    vtkRenderer* layer1,
    vtkRenderer* layer2,
    vtkRenderer* layer3,
    QObject* parent)
    : NodeDisplayManager(scene, windowId, layer1, layer2, layer3, parent)
{
    m_renderCallback = vtkSmartPointer<vtkCallbackCommand>::New();
    m_renderCallback->SetClientData(this);
    m_renderCallback->SetCallback([](vtkObject* caller, unsigned long, void* clientData, void*) {
        auto* self = static_cast<BillboardArrowNodeDisplayManager*>(clientData);
        self->onRendererStart(vtkRenderer::SafeDownCast(caller));
    });

    for (int layer = 1; layer <= 3; ++layer) {
        if (vtkRenderer* renderer = getRenderer(layer)) {
            m_rendererObserverTags[layer - 1] = renderer->AddObserver(vtkCommand::StartEvent, m_renderCallback);
        }
    }
}

BillboardArrowNodeDisplayManager::~BillboardArrowNodeDisplayManager()
{
    detachRendererObservers();
    clearAll();
}

void BillboardArrowNodeDisplayManager::detachRendererObservers()
{
    for (int layer = 1; layer <= 3; ++layer) {
        vtkRenderer* renderer = getRenderer(layer);
        const unsigned long tag = m_rendererObserverTags[layer - 1];
        if (renderer && tag != 0) {
            renderer->RemoveObserver(tag);
        }
        m_rendererObserverTags[layer - 1] = 0;
    }

    m_renderCallback = nullptr;
}

bool BillboardArrowNodeDisplayManager::canHandleNode(NodeBase* node) const
{
    return dynamic_cast<BillboardArrowNode*>(node) != nullptr;
}

void BillboardArrowNodeDisplayManager::onNodeAdded(const QString& nodeId)
{
    if (m_entries.contains(nodeId)) {
        return;
    }

    buildEntry(nodeId);
}

void BillboardArrowNodeDisplayManager::onNodeRemoved(const QString& nodeId)
{
    removeEntry(nodeId);
}

void BillboardArrowNodeDisplayManager::onNodeModified(const QString& nodeId,
                                                      NodeEventType eventType)
{
    if (!m_entries.contains(nodeId)) {
        buildEntry(nodeId);
        return;
    }

    switch (eventType) {
    case NodeEventType::ContentModified:
    case NodeEventType::TransformChanged:
        updateDisplay(nodeId);
        break;
    case NodeEventType::DisplayChanged:
        updateDisplay(nodeId);
        break;
    default:
        updateDisplay(nodeId);
        break;
    }
}

void BillboardArrowNodeDisplayManager::reconcileWithScene()
{
    const QVector<BillboardArrowNode*> sceneNodes = scene()->getAllBillboardArrowNodes();
    QSet<QString> sceneIds;
    for (BillboardArrowNode* node : sceneNodes) {
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

    for (BillboardArrowNode* node : sceneNodes) {
        if (!node) {
            continue;
        }

        const QString nodeId = node->getNodeId();
        if (!m_entries.contains(nodeId)) {
            buildEntry(nodeId);
            continue;
        }

        updateDisplay(nodeId);
    }
}

void BillboardArrowNodeDisplayManager::clearAll()
{
    const QStringList nodeIds = m_entries.keys();
    for (const QString& nodeId : nodeIds) {
        removeEntry(nodeId);
    }
}

void BillboardArrowNodeDisplayManager::buildEntry(const QString& nodeId)
{
    BillboardArrowNode* node = scene()->getNodeById<BillboardArrowNode>(nodeId);
    if (!node) {
        return;
    }

    ArrowDisplayEntry entry;
    entry.currentLayer = getNodeLayerInWindow(node);
    entry.cachedDirection = node->getDirection();
    entry.cachedFollowCameraRotation = node->isFollowCameraRotation();
    entry.points = vtkSmartPointer<vtkPoints>::New();
    entry.polygons = vtkSmartPointer<vtkCellArray>::New();
    entry.lines = vtkSmartPointer<vtkCellArray>::New();
    entry.polyData = vtkSmartPointer<vtkPolyData>::New();
    entry.polyData->SetPoints(entry.points);
    entry.polyData->SetPolys(entry.polygons);
    entry.polyData->SetLines(entry.lines);
    entry.mapper = vtkSmartPointer<vtkPolyDataMapper2D>::New();
    entry.mapper->SetInputData(entry.polyData);
    entry.actor = vtkSmartPointer<vtkActor2D>::New();
    entry.actor->SetMapper(entry.mapper);
    entry.actor->SetVisibility(0);

    if (vtkRenderer* renderer = getRenderer(entry.currentLayer)) {
        renderer->AddActor2D(entry.actor);
    }

    m_entries.insert(nodeId, entry);
    updateDisplay(nodeId);
}

void BillboardArrowNodeDisplayManager::removeEntry(const QString& nodeId)
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

void BillboardArrowNodeDisplayManager::updateDisplay(const QString& nodeId)
{
    BillboardArrowNode* node = scene()->getNodeById<BillboardArrowNode>(nodeId);
    auto it = m_entries.find(nodeId);
    if (!node || it == m_entries.end()) {
        return;
    }

    ArrowDisplayEntry& entry = it.value();
    vtkProperty2D* property = entry.actor->GetProperty();
    double color[4];
    node->getColor(color);
    property->SetColor(color[0], color[1], color[2]);
    property->SetOpacity(node->getOpacity());
    property->SetLineWidth(static_cast<float>(node->getShaftWidth()));

    const QString direction = node->getDirection();
    const bool followCameraRotation = node->isFollowCameraRotation();
    if (entry.cachedDirection != direction ||
        entry.cachedFollowCameraRotation != followCameraRotation) {
        entry.cachedDirection = direction;
        entry.cachedFollowCameraRotation = followCameraRotation;
        entry.hasCapturedWorldDirection = false;
    }

    const int newLayer = getNodeLayerInWindow(node);
    if (newLayer != entry.currentLayer) {
        if (vtkRenderer* oldRenderer = getRenderer(entry.currentLayer)) {
            oldRenderer->RemoveActor2D(entry.actor);
        }
        if (vtkRenderer* newRenderer = getRenderer(newLayer)) {
            newRenderer->AddActor2D(entry.actor);
        }
        entry.currentLayer = newLayer;
        entry.hasCapturedWorldDirection = false;
    }

    if (!isNodeVisibleInWindow(node)) {
        entry.actor->SetVisibility(0);
        return;
    }

    updateDisplayPosition(nodeId, getRenderer(entry.currentLayer));
}

void BillboardArrowNodeDisplayManager::updateDisplayPosition(const QString& nodeId, vtkRenderer* renderer)
{
    BillboardArrowNode* node = scene()->getNodeById<BillboardArrowNode>(nodeId);
    auto it = m_entries.find(nodeId);
    if (!node || it == m_entries.end() || !renderer) {
        return;
    }

    ArrowDisplayEntry& entry = it.value();
    Vector3 tipPoint = node->getTipPoint();
    double worldMatrix[16];
    if (scene()->getWorldTransformMatrix(nodeId, worldMatrix)) {
        tipPoint = transformPoint(worldMatrix, tipPoint);
    }

    renderer->SetWorldPoint(tipPoint[0], tipPoint[1], tipPoint[2], 1.0);
    renderer->WorldToDisplay();
    const double* tipDisplayPoint = renderer->GetDisplayPoint();
    double tipDisplay[3] = {
        tipDisplayPoint ? tipDisplayPoint[0] : 0.0,
        tipDisplayPoint ? tipDisplayPoint[1] : 0.0,
        tipDisplayPoint ? tipDisplayPoint[2] : 0.0};

    const bool visible = isNodeVisibleInWindow(node) &&
        tipDisplay[2] >= 0.0 && tipDisplay[2] <= 1.0;
    entry.actor->SetVisibility(visible ? 1 : 0);
    if (!visible) {
        return;
    }

    std::array<double, 2> directionDisplay = defaultDisplayDirection(node->getDirection());
    if (node->isFollowCameraRotation()) {
        if (!entry.hasCapturedWorldDirection) {
            vtkCamera* camera = renderer->GetActiveCamera();
            if (camera) {
                Vector3 upWorld = normalizeVec3(
                    {camera->GetViewUp()[0], camera->GetViewUp()[1], camera->GetViewUp()[2]},
                    {0.0, 1.0, 0.0});
                const Vector3 capturedDirection = node->getDirection() == QStringLiteral("up")
                    ? scaleVec3(upWorld, -1.0)
                    : upWorld;
                entry.capturedWorldDirection[0] = capturedDirection[0];
                entry.capturedWorldDirection[1] = capturedDirection[1];
                entry.capturedWorldDirection[2] = capturedDirection[2];
                entry.hasCapturedWorldDirection = true;
            }
        }

        if (entry.hasCapturedWorldDirection) {
            const Vector3 probeWorld = addVec3(
                tipPoint,
                scaleVec3({entry.capturedWorldDirection[0],
                           entry.capturedWorldDirection[1],
                           entry.capturedWorldDirection[2]},
                          kOrientationProbeLength));

            renderer->SetWorldPoint(probeWorld[0], probeWorld[1], probeWorld[2], 1.0);
            renderer->WorldToDisplay();
            const double* probeDisplayPoint = renderer->GetDisplayPoint();
            double probeDisplay[3] = {
                probeDisplayPoint ? probeDisplayPoint[0] : 0.0,
                probeDisplayPoint ? probeDisplayPoint[1] : 0.0,
                probeDisplayPoint ? probeDisplayPoint[2] : 0.0};

            const double dx = probeDisplay[0] - tipDisplay[0];
            const double dy = probeDisplay[1] - tipDisplay[1];
            const double displayLength = std::sqrt(dx * dx + dy * dy);
            if (displayLength > 1e-6) {
                directionDisplay = {dx / displayLength, dy / displayLength};
            }
        }
    }

    entry.points->Reset();
    entry.polygons->Reset();
    entry.lines->Reset();

    const double directionX = directionDisplay[0];
    const double directionY = directionDisplay[1];
    const double rightX = -directionY;
    const double rightY = directionX;

    auto toDisplay = [&](double localX, double localY) {
        return std::array<double, 3>{
            tipDisplay[0] + rightX * localX + directionX * localY,
            tipDisplay[1] + rightY * localX + directionY * localY,
            0.0};
    };

    const auto tip = toDisplay(0.0, 0.0);
    const auto leftBase = toDisplay(-0.5 * node->getHeadWidth(), node->getHeadLength());
    const auto rightBase = toDisplay(0.5 * node->getHeadWidth(), node->getHeadLength());
    const auto shaftStart = toDisplay(0.0, node->getHeadLength());
    const auto shaftEnd = toDisplay(0.0, node->getHeadLength() + node->getShaftLength());

    const vtkIdType tipIndex = entry.points->InsertNextPoint(tip[0], tip[1], tip[2]);
    const vtkIdType leftIndex = entry.points->InsertNextPoint(leftBase[0], leftBase[1], leftBase[2]);
    const vtkIdType rightIndex = entry.points->InsertNextPoint(rightBase[0], rightBase[1], rightBase[2]);
    const vtkIdType shaftStartIndex = entry.points->InsertNextPoint(shaftStart[0], shaftStart[1], shaftStart[2]);
    const vtkIdType shaftEndIndex = entry.points->InsertNextPoint(shaftEnd[0], shaftEnd[1], shaftEnd[2]);

    vtkSmartPointer<vtkTriangle> triangle = vtkSmartPointer<vtkTriangle>::New();
    triangle->GetPointIds()->SetId(0, tipIndex);
    triangle->GetPointIds()->SetId(1, leftIndex);
    triangle->GetPointIds()->SetId(2, rightIndex);
    entry.polygons->InsertNextCell(triangle);

    vtkIdType shaftIds[2] = {shaftStartIndex, shaftEndIndex};
    entry.lines->InsertNextCell(2, shaftIds);

    entry.points->Modified();
    entry.polygons->Modified();
    entry.lines->Modified();
    entry.polyData->Modified();
}

void BillboardArrowNodeDisplayManager::onRendererStart(vtkRenderer* renderer)
{
    if (!renderer) {
        return;
    }

    const int layer = renderer->GetLayer() + 1;
    for (auto it = m_entries.begin(); it != m_entries.end(); ++it) {
        if (it->currentLayer != layer) {
            continue;
        }
        updateDisplayPosition(it.key(), renderer);
    }
}