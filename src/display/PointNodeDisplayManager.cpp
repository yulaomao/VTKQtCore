#include "PointNodeDisplayManager.h"
#include "../logic/scene/nodes/PointNode.h"

#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkSphereSource.h>
#include <vtkGlyph3DMapper.h>
#include <vtkPoints.h>
#include <vtkProperty.h>
#include <vtkLabeledDataMapper.h>
#include <vtkStringArray.h>
#include <vtkCellArray.h>
#include <vtkPointData.h>
#include <vtkRenderer.h>
#include <vtkTextProperty.h>
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

PointNodeDisplayManager::PointNodeDisplayManager(SceneGraph* scene, const QString& windowId,
                                                   vtkRenderer* layer1, vtkRenderer* layer2,
                                                   vtkRenderer* layer3, QObject* parent)
    : NodeDisplayManager(scene, windowId, layer1, layer2, layer3, parent)
{
}

PointNodeDisplayManager::~PointNodeDisplayManager()
{
    clearAll();
}

bool PointNodeDisplayManager::canHandleNode(NodeBase* node) const
{
    return dynamic_cast<PointNode*>(node) != nullptr;
}

void PointNodeDisplayManager::onNodeAdded(const QString& nodeId)
{
    if (m_entries.contains(nodeId)) {
        return;
    }
    buildEntry(nodeId);
}

void PointNodeDisplayManager::onNodeRemoved(const QString& nodeId)
{
    removeEntry(nodeId);
}

void PointNodeDisplayManager::onNodeModified(const QString& nodeId, NodeEventType eventType)
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
        updateContent(nodeId);
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

void PointNodeDisplayManager::reconcileWithScene()
{
    QVector<PointNode*> sceneNodes = scene()->getAllPointNodes();
    QSet<QString> sceneIds;
    for (PointNode* pn : sceneNodes) {
        sceneIds.insert(pn->getNodeId());
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
    for (PointNode* pn : sceneNodes) {
        const QString& id = pn->getNodeId();
        if (!m_entries.contains(id) && canHandleNode(pn)) {
            buildEntry(id);
        } else if (m_entries.contains(id)) {
            updateContent(id);
            updateDisplay(id);
            updateTransform(id);
        }
    }
}

void PointNodeDisplayManager::clearAll()
{
    QStringList ids = m_entries.keys();
    for (const QString& id : ids) {
        removeEntry(id);
    }
}

void PointNodeDisplayManager::populatePointData(PointNode* node,
                                                vtkPoints* points,
                                                vtkCellArray* verts,
                                                vtkStringArray* labels,
                                                vtkPolyData* polyData) const
{
    if (!node || !points || !verts || !labels || !polyData) {
        return;
    }

    points->Reset();
    verts->Reset();
    labels->Reset();

    int count = node->getPointCount();
    for (int i = 0; i < count; ++i) {
        double pos[3];
        node->getPointPosition(i, pos);
        vtkIdType pid = points->InsertNextPoint(pos);
        verts->InsertNextCell(1, &pid);

        const PointItem& item = node->getPointByIndex(i);
        labels->InsertNextValue(item.label.toStdString());
    }

    polyData->SetPoints(points);
    polyData->SetVerts(verts);
    points->Modified();
    verts->Modified();
    labels->Modified();
    polyData->Modified();
}

void PointNodeDisplayManager::buildEntry(const QString& nodeId)
{
    PointNode* node = scene()->getNodeById<PointNode>(nodeId);
    if (!node) {
        return;
    }

    bool visible = isNodeVisibleInWindow(node);
    int layer = getNodeLayerInWindow(node);

    PointDisplayEntry entry;
    entry.points = vtkSmartPointer<vtkPoints>::New();
    entry.verts = vtkSmartPointer<vtkCellArray>::New();
    entry.labels = vtkSmartPointer<vtkStringArray>::New();
    entry.labels->SetName("Labels");
    entry.polyData = vtkSmartPointer<vtkPolyData>::New();
    entry.polyData->GetPointData()->AddArray(entry.labels);
    populatePointData(node, entry.points, entry.verts, entry.labels, entry.polyData);

    entry.sphere = vtkSmartPointer<vtkSphereSource>::New();
    double defaultSize = node->getDefaultPointSize();
    entry.sphere->SetRadius(defaultSize * 0.5);
    entry.sphere->SetThetaResolution(12);
    entry.sphere->SetPhiResolution(12);
    entry.sphere->Update();

    entry.glyphMapper = vtkSmartPointer<vtkGlyph3DMapper>::New();
    entry.glyphMapper->SetInputData(entry.polyData);
    entry.glyphMapper->SetSourceData(entry.sphere->GetOutput());
    entry.glyphMapper->ScalingOff();
    entry.glyphMapper->Update();

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(entry.glyphMapper);

    double color[4];
    node->getDefaultPointColor(color);
    actor->GetProperty()->SetColor(color[0], color[1], color[2]);
    actor->GetProperty()->SetOpacity(color[3]);
    actor->SetVisibility(visible ? 1 : 0);

    entry.labelMapper = vtkSmartPointer<vtkLabeledDataMapper>::New();
    entry.labelMapper->SetInputData(entry.polyData);
    entry.labelMapper->SetLabelModeToLabelFieldData();
    entry.labelMapper->SetFieldDataName("Labels");
    entry.labelMapper->GetLabelTextProperty()->SetFontSize(14);
    entry.labelMapper->GetLabelTextProperty()->SetColor(1.0, 1.0, 1.0);
    entry.labelMapper->GetLabelTextProperty()->SetJustificationToCentered();
    entry.labelMapper->GetLabelTextProperty()->SetVerticalJustificationToBottom();
    entry.labelMapper->GetLabelTextProperty()->SetBold(1);
    entry.labelMapper->GetLabelTextProperty()->SetShadow(1);

    auto labelActor = vtkSmartPointer<vtkActor2D>::New();
    labelActor->SetMapper(entry.labelMapper);
    bool showLabels = node->isShowPointLabel() && visible;
    labelActor->SetVisibility(showLabels ? 1 : 0);

    // Add to renderer
    vtkRenderer* renderer = getRenderer(layer);
    if (renderer) {
        renderer->AddActor(actor);
        renderer->AddActor2D(labelActor);
    }

    entry.actor = actor;
    entry.labelActor = labelActor;
    entry.transform = vtkSmartPointer<vtkTransform>::New();
    entry.transformMatrix = vtkSmartPointer<vtkMatrix4x4>::New();
    copyArray(entry.color, color);
    entry.sphereRadius = defaultSize * 0.5;
    entry.visible = visible;
    entry.labelsVisible = showLabels;
    entry.currentLayer = layer;
    m_entries.insert(nodeId, entry);
}

void PointNodeDisplayManager::removeEntry(const QString& nodeId)
{
    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    vtkRenderer* renderer = getRenderer(it->currentLayer);
    if (renderer) {
        if (it->actor) {
            renderer->RemoveActor(it->actor);
        }
        if (it->labelActor) {
            renderer->RemoveActor2D(it->labelActor);
        }
    }
    m_entries.erase(it);
}

void PointNodeDisplayManager::updateContent(const QString& nodeId)
{
    PointNode* node = scene()->getNodeById<PointNode>(nodeId);
    if (!node) {
        return;
    }

    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    populatePointData(node, it->points, it->verts, it->labels, it->polyData);

    const double sphereRadius = node->getDefaultPointSize() * 0.5;
    if (it->sphere && !nearlyEqual(it->sphereRadius, sphereRadius)) {
        it->sphere->SetRadius(sphereRadius);
        it->sphere->Update();
        it->sphereRadius = sphereRadius;
    }
    if (it->glyphMapper) {
        it->glyphMapper->Modified();
    }
    if (it->labelMapper) {
        it->labelMapper->Modified();
    }
}

void PointNodeDisplayManager::updateDisplay(const QString& nodeId)
{
    PointNode* node = scene()->getNodeById<PointNode>(nodeId);
    if (!node) {
        return;
    }

    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    bool visible = isNodeVisibleInWindow(node);
    int newLayer = getNodeLayerInWindow(node);

    // Update color and opacity
    double color[4];
    node->getDefaultPointColor(color);
    if (!arrayEquals(it->color, color)) {
        it->actor->GetProperty()->SetColor(color[0], color[1], color[2]);
        it->actor->GetProperty()->SetOpacity(color[3]);
        copyArray(it->color, color);
    }
    if (it->visible != visible) {
        it->actor->SetVisibility(visible ? 1 : 0);
        it->visible = visible;
    }

    bool showLabels = node->isShowPointLabel() && visible;
    if (it->labelsVisible != showLabels) {
        it->labelActor->SetVisibility(showLabels ? 1 : 0);
        it->labelsVisible = showLabels;
    }

    // Handle layer change
    if (newLayer != it->currentLayer) {
        vtkRenderer* oldRenderer = getRenderer(it->currentLayer);
        vtkRenderer* newRenderer = getRenderer(newLayer);
        if (oldRenderer) {
            oldRenderer->RemoveActor(it->actor);
            oldRenderer->RemoveActor2D(it->labelActor);
        }
        if (newRenderer) {
            newRenderer->AddActor(it->actor);
            newRenderer->AddActor2D(it->labelActor);
        }
        it->currentLayer = newLayer;
    }
}

void PointNodeDisplayManager::updateTransform(const QString& nodeId)
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
