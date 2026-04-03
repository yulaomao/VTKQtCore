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

void PointNodeDisplayManager::buildEntry(const QString& nodeId)
{
    PointNode* node = scene()->getNodeById<PointNode>(nodeId);
    if (!node) {
        return;
    }

    int layer = getNodeLayerInWindow(node);

    PointDisplayEntry entry;
    entry.currentLayer = layer;
    entry.points = vtkSmartPointer<vtkPoints>::New();
    entry.verts = vtkSmartPointer<vtkCellArray>::New();
    entry.labels = vtkSmartPointer<vtkStringArray>::New();
    entry.labels->SetName("Labels");
    entry.polyData = vtkSmartPointer<vtkPolyData>::New();
    entry.polyData->SetPoints(entry.points);
    entry.polyData->SetVerts(entry.verts);
    entry.polyData->GetPointData()->AddArray(entry.labels);

    entry.sphere = vtkSmartPointer<vtkSphereSource>::New();
    entry.sphere->SetThetaResolution(12);
    entry.sphere->SetPhiResolution(12);
    entry.sphere->SetRadius(0.5);
    entry.sphere->Update();

    entry.glyphMapper = vtkSmartPointer<vtkGlyph3DMapper>::New();
    entry.glyphMapper->SetInputData(entry.polyData);
    entry.glyphMapper->SetSourceData(entry.sphere->GetOutput());
    entry.glyphMapper->ScalingOff();

    entry.actor = vtkSmartPointer<vtkActor>::New();
    entry.actor->SetMapper(entry.glyphMapper);
    entry.actor->SetVisibility(0);

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

    entry.labelActor = vtkSmartPointer<vtkActor2D>::New();
    entry.labelActor->SetMapper(entry.labelMapper);
    entry.labelActor->SetVisibility(0);

    vtkRenderer* renderer = getRenderer(layer);
    if (renderer) {
        renderer->AddActor(entry.actor);
        renderer->AddActor2D(entry.labelActor);
    }

    m_entries.insert(nodeId, entry);
    updateContent(nodeId);
    updateDisplay(nodeId);
    updateTransform(nodeId);
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

    PointDisplayEntry& entry = it.value();

    entry.points->Reset();
    entry.verts->Reset();
    entry.labels->Reset();

    int count = node->getPointCount();
    for (int i = 0; i < count; ++i) {
        double pos[3];
        node->getPointPosition(i, pos);
        vtkIdType pid = entry.points->InsertNextPoint(pos);
        entry.verts->InsertNextCell(1, &pid);

        const PointItem& item = node->getPointByIndex(i);
        entry.labels->InsertNextValue(item.label.toStdString());
    }

    entry.points->Modified();
    entry.verts->Modified();
    entry.labels->Modified();
    entry.polyData->Modified();

    double defaultSize = node->getDefaultPointSize();
    double radius = defaultSize * 0.5;
    if (!entry.hasRadius || !areScalarsEqual(entry.cachedRadius, radius)) {
        entry.sphere->SetRadius(radius);
        entry.sphere->Update();
        entry.cachedRadius = radius;
        entry.hasRadius = true;
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

    PointDisplayEntry& entry = it.value();

    bool visible = isNodeVisibleInWindow(node);
    int newLayer = getNodeLayerInWindow(node);

    double color[4];
    node->getDefaultPointColor(color);
    if (!entry.hasDisplayColor || !areArraysEqual(entry.cachedDisplayColor, color, 4)) {
        entry.actor->GetProperty()->SetColor(color[0], color[1], color[2]);
        entry.actor->GetProperty()->SetOpacity(color[3]);
        copyArray(color, entry.cachedDisplayColor, 4);
        entry.hasDisplayColor = true;
    }

    if (entry.actorVisible != visible) {
        entry.actor->SetVisibility(visible ? 1 : 0);
        entry.actorVisible = visible;
    }

    bool showLabels = node->isShowPointLabel() && visible;
    if (entry.labelVisible != showLabels) {
        entry.labelActor->SetVisibility(showLabels ? 1 : 0);
        entry.labelVisible = showLabels;
    }

    if (newLayer != entry.currentLayer) {
        vtkRenderer* oldRenderer = getRenderer(entry.currentLayer);
        vtkRenderer* newRenderer = getRenderer(newLayer);
        if (oldRenderer) {
            oldRenderer->RemoveActor(entry.actor);
            oldRenderer->RemoveActor2D(entry.labelActor);
        }
        if (newRenderer) {
            newRenderer->AddActor(entry.actor);
            newRenderer->AddActor2D(entry.labelActor);
        }
        entry.currentLayer = newLayer;
    }
}

void PointNodeDisplayManager::updateTransform(const QString& nodeId)
{
    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    PointDisplayEntry& entry = it.value();

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
