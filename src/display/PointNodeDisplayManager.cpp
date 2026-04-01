#include "PointNodeDisplayManager.h"
#include "../logic/scene/nodes/PointNode.h"
#include "../logic/scene/nodes/TransformNode.h"

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

    bool visible = isNodeVisibleInWindow(node);
    int layer = getNodeLayerInWindow(node);

    // Build point polydata
    auto points = vtkSmartPointer<vtkPoints>::New();
    auto verts = vtkSmartPointer<vtkCellArray>::New();
    auto labels = vtkSmartPointer<vtkStringArray>::New();
    labels->SetName("Labels");

    int count = node->getPointCount();
    for (int i = 0; i < count; ++i) {
        double pos[3];
        node->getPointPosition(i, pos);
        vtkIdType pid = points->InsertNextPoint(pos);
        verts->InsertNextCell(1, &pid);

        const PointItem& item = node->getPointByIndex(i);
        labels->InsertNextValue(item.label.toStdString());
    }

    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetVerts(verts);
    polyData->GetPointData()->AddArray(labels);

    // Glyph mapper with sphere source
    auto sphere = vtkSmartPointer<vtkSphereSource>::New();
    double defaultSize = node->getDefaultPointSize();
    sphere->SetRadius(defaultSize * 0.5);
    sphere->SetThetaResolution(12);
    sphere->SetPhiResolution(12);
    sphere->Update();

    auto glyphMapper = vtkSmartPointer<vtkGlyph3DMapper>::New();
    glyphMapper->SetInputData(polyData);
    glyphMapper->SetSourceData(sphere->GetOutput());
    glyphMapper->ScalingOff();
    glyphMapper->Update();

    auto actor = vtkSmartPointer<vtkActor>::New();
    actor->SetMapper(glyphMapper);

    double color[4];
    node->getDefaultPointColor(color);
    actor->GetProperty()->SetColor(color[0], color[1], color[2]);
    actor->GetProperty()->SetOpacity(color[3]);
    actor->SetVisibility(visible ? 1 : 0);

    // Label actor
    auto labelMapper = vtkSmartPointer<vtkLabeledDataMapper>::New();
    labelMapper->SetInputData(polyData);
    labelMapper->SetLabelModeToLabelFieldData();
    labelMapper->SetFieldDataName("Labels");
    labelMapper->GetLabelTextProperty()->SetFontSize(14);
    labelMapper->GetLabelTextProperty()->SetColor(1.0, 1.0, 1.0);
    labelMapper->GetLabelTextProperty()->SetJustificationToCentered();
    labelMapper->GetLabelTextProperty()->SetVerticalJustificationToBottom();
    labelMapper->GetLabelTextProperty()->SetBold(1);
    labelMapper->GetLabelTextProperty()->SetShadow(1);

    auto labelActor = vtkSmartPointer<vtkActor2D>::New();
    labelActor->SetMapper(labelMapper);
    bool showLabels = node->isShowPointLabel() && visible;
    labelActor->SetVisibility(showLabels ? 1 : 0);

    // Add to renderer
    vtkRenderer* renderer = getRenderer(layer);
    if (renderer) {
        renderer->AddActor(actor);
        renderer->AddActor2D(labelActor);
    }

    PointDisplayEntry entry;
    entry.actor = actor;
    entry.labelActor = labelActor;
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

    auto points = vtkSmartPointer<vtkPoints>::New();
    auto verts = vtkSmartPointer<vtkCellArray>::New();
    auto labels = vtkSmartPointer<vtkStringArray>::New();
    labels->SetName("Labels");

    int count = node->getPointCount();
    for (int i = 0; i < count; ++i) {
        double pos[3];
        node->getPointPosition(i, pos);
        vtkIdType pid = points->InsertNextPoint(pos);
        verts->InsertNextCell(1, &pid);

        const PointItem& item = node->getPointByIndex(i);
        labels->InsertNextValue(item.label.toStdString());
    }

    auto polyData = vtkSmartPointer<vtkPolyData>::New();
    polyData->SetPoints(points);
    polyData->SetVerts(verts);
    polyData->GetPointData()->AddArray(labels);

    // Update sphere radius
    auto sphere = vtkSmartPointer<vtkSphereSource>::New();
    double defaultSize = node->getDefaultPointSize();
    sphere->SetRadius(defaultSize * 0.5);
    sphere->SetThetaResolution(12);
    sphere->SetPhiResolution(12);
    sphere->Update();

    auto glyphMapper = vtkSmartPointer<vtkGlyph3DMapper>::New();
    glyphMapper->SetInputData(polyData);
    glyphMapper->SetSourceData(sphere->GetOutput());
    glyphMapper->ScalingOff();
    glyphMapper->Update();

    it->actor->SetMapper(glyphMapper);

    // Update label mapper
    auto labelMapper = vtkSmartPointer<vtkLabeledDataMapper>::New();
    labelMapper->SetInputData(polyData);
    labelMapper->SetLabelModeToLabelFieldData();
    labelMapper->SetFieldDataName("Labels");
    labelMapper->GetLabelTextProperty()->SetFontSize(14);
    labelMapper->GetLabelTextProperty()->SetColor(1.0, 1.0, 1.0);
    labelMapper->GetLabelTextProperty()->SetJustificationToCentered();
    labelMapper->GetLabelTextProperty()->SetVerticalJustificationToBottom();
    labelMapper->GetLabelTextProperty()->SetBold(1);
    labelMapper->GetLabelTextProperty()->SetShadow(1);

    it->labelActor->SetMapper(labelMapper);
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
    it->actor->GetProperty()->SetColor(color[0], color[1], color[2]);
    it->actor->GetProperty()->SetOpacity(color[3]);
    it->actor->SetVisibility(visible ? 1 : 0);

    bool showLabels = node->isShowPointLabel() && visible;
    it->labelActor->SetVisibility(showLabels ? 1 : 0);

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
    PointNode* node = scene()->getNodeById<PointNode>(nodeId);
    if (!node) {
        return;
    }

    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    QString transformId = node->getParentTransform();
    if (transformId.isEmpty()) {
        it->actor->SetUserTransform(nullptr);
        return;
    }

    TransformNode* transformNode = scene()->getNodeById<TransformNode>(transformId);
    if (!transformNode) {
        it->actor->SetUserTransform(nullptr);
        return;
    }

    double matrix[16];
    transformNode->getMatrixTransformToParent(matrix);

    auto transform = vtkSmartPointer<vtkTransform>::New();
    auto mat = vtkSmartPointer<vtkMatrix4x4>::New();
    mat->DeepCopy(matrix);
    transform->SetMatrix(mat);

    it->actor->SetUserTransform(transform);
}
