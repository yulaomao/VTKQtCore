#include "PlaneNodeDisplayManager.h"

#include "../logic/scene/nodes/PlaneNode.h"

#include <vtkMatrix4x4.h>
#include <vtkPolyDataMapper.h>
#include <vtkProperty.h>
#include <vtkRenderer.h>
#include <vtkTransform.h>

#include <QSet>

#include <cmath>

namespace {

constexpr double kTolerance = 1e-12;

bool areScalarsEqual(double lhs, double rhs)
{
    return std::abs(lhs - rhs) <= kTolerance;
}

bool areArraysEqual(const double* lhs, const double* rhs, int count)
{
    for (int index = 0; index < count; ++index) {
        if (!areScalarsEqual(lhs[index], rhs[index])) {
            return false;
        }
    }
    return true;
}

void copyArray(const double* source, double* target, int count)
{
    for (int index = 0; index < count; ++index) {
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

PlaneNodeDisplayManager::PlaneNodeDisplayManager(SceneGraph* scene, const QString& windowId,
                                                 vtkRenderer* layer1, vtkRenderer* layer2,
                                                 vtkRenderer* layer3, QObject* parent)
    : NodeDisplayManager(scene, windowId, layer1, layer2, layer3, parent)
{
}

PlaneNodeDisplayManager::~PlaneNodeDisplayManager()
{
    clearAll();
}

bool PlaneNodeDisplayManager::canHandleNode(NodeBase* node) const
{
    return dynamic_cast<PlaneNode*>(node) != nullptr;
}

void PlaneNodeDisplayManager::onNodeAdded(const QString& nodeId)
{
    if (m_entries.contains(nodeId)) {
        return;
    }

    buildEntry(nodeId);
}

void PlaneNodeDisplayManager::onNodeRemoved(const QString& nodeId)
{
    removeEntry(nodeId);
}

void PlaneNodeDisplayManager::onNodeModified(const QString& nodeId, NodeEventType eventType)
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

void PlaneNodeDisplayManager::reconcileWithScene()
{
    const QVector<PlaneNode*> sceneNodes = scene()->getAllPlaneNodes();
    QSet<QString> sceneIds;
    for (PlaneNode* node : sceneNodes) {
        sceneIds.insert(node->getNodeId());
    }

    QStringList staleIds;
    for (auto it = m_entries.constBegin(); it != m_entries.constEnd(); ++it) {
        if (!sceneIds.contains(it.key())) {
            staleIds.append(it.key());
        }
    }
    for (const QString& staleId : staleIds) {
        removeEntry(staleId);
    }

    for (PlaneNode* node : sceneNodes) {
        const QString nodeId = node->getNodeId();
        if (!m_entries.contains(nodeId) && canHandleNode(node)) {
            buildEntry(nodeId);
        } else if (m_entries.contains(nodeId)) {
            updateContent(nodeId);
            updateDisplay(nodeId);
            updateTransform(nodeId);
        }
    }
}

void PlaneNodeDisplayManager::clearAll()
{
    const QStringList ids = m_entries.keys();
    for (const QString& id : ids) {
        removeEntry(id);
    }
}

void PlaneNodeDisplayManager::buildEntry(const QString& nodeId)
{
    auto* node = scene()->getNodeById<PlaneNode>(nodeId);
    if (!node) {
        return;
    }

    PlaneDisplayEntry entry;
    entry.fillMapper = vtkSmartPointer<vtkPolyDataMapper>::New();
    entry.borderMapper = vtkSmartPointer<vtkPolyDataMapper>::New();

    entry.fillActor = vtkSmartPointer<vtkActor>::New();
    entry.fillActor->SetMapper(entry.fillMapper);
    entry.fillActor->GetProperty()->SetRepresentationToSurface();
    entry.fillActor->GetProperty()->LightingOff();

    entry.borderActor = vtkSmartPointer<vtkActor>::New();
    entry.borderActor->SetMapper(entry.borderMapper);
    entry.borderActor->GetProperty()->SetRepresentationToWireframe();
    entry.borderActor->GetProperty()->LightingOff();

    vtkRenderer* renderer = getRenderer(getNodeLayerInWindow(node));
    if (renderer) {
        renderer->AddActor(entry.fillActor);
        renderer->AddActor(entry.borderActor);
        entry.currentLayer = getNodeLayerInWindow(node);
    }

    m_entries.insert(nodeId, entry);
    updateContent(nodeId);
    updateDisplay(nodeId);
    updateTransform(nodeId);
}

void PlaneNodeDisplayManager::removeEntry(const QString& nodeId)
{
    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    PlaneDisplayEntry& entry = it.value();
    if (vtkRenderer* renderer = getRenderer(entry.currentLayer)) {
        renderer->RemoveActor(entry.fillActor);
        renderer->RemoveActor(entry.borderActor);
    }
    m_entries.erase(it);
}

void PlaneNodeDisplayManager::updateContent(const QString& nodeId)
{
    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    auto* node = scene()->getNodeById<PlaneNode>(nodeId);
    if (!node) {
        removeEntry(nodeId);
        return;
    }

    PlaneDisplayEntry& entry = it.value();
    vtkSmartPointer<vtkPolyData> newSurface = node->getSurfacePolyData();
    vtkSmartPointer<vtkPolyData> newBorder = node->getBorderPolyData();
    if (entry.surfacePolyData != newSurface) {
        entry.surfacePolyData = newSurface;
        entry.fillMapper->SetInputData(entry.surfacePolyData);
        entry.fillMapper->Update();
    }
    if (entry.borderPolyData != newBorder) {
        entry.borderPolyData = newBorder;
        entry.borderMapper->SetInputData(entry.borderPolyData);
        entry.borderMapper->Update();
    }
}

void PlaneNodeDisplayManager::updateDisplay(const QString& nodeId)
{
    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    auto* node = scene()->getNodeById<PlaneNode>(nodeId);
    if (!node) {
        removeEntry(nodeId);
        return;
    }

    PlaneDisplayEntry& entry = it.value();
    vtkProperty* fillProp = entry.fillActor->GetProperty();
    vtkProperty* borderProp = entry.borderActor->GetProperty();

    double fillColor[4];
    node->getPlaneColor(fillColor);
    if (!entry.hasFillColor || !areArraysEqual(entry.cachedFillColor, fillColor, 4)) {
        fillProp->SetColor(fillColor[0], fillColor[1], fillColor[2]);
        copyArray(fillColor, entry.cachedFillColor, 4);
        entry.hasFillColor = true;
    }

    const double fillOpacity = node->getPlaneOpacity();
    if (!entry.hasFillOpacity || !areScalarsEqual(entry.cachedFillOpacity, fillOpacity)) {
        fillProp->SetOpacity(fillOpacity);
        entry.cachedFillOpacity = fillOpacity;
        entry.hasFillOpacity = true;
    }

    double borderColor[4];
    node->getBorderColor(borderColor);
    if (!entry.hasBorderColor || !areArraysEqual(entry.cachedBorderColor, borderColor, 4)) {
        borderProp->SetColor(borderColor[0], borderColor[1], borderColor[2]);
        copyArray(borderColor, entry.cachedBorderColor, 4);
        entry.hasBorderColor = true;
    }

    const double borderOpacity = node->getBorderOpacity();
    if (!entry.hasBorderOpacity || !areScalarsEqual(entry.cachedBorderOpacity, borderOpacity)) {
        borderProp->SetOpacity(borderOpacity);
        entry.cachedBorderOpacity = borderOpacity;
        entry.hasBorderOpacity = true;
    }

    const double borderWidth = node->getBorderWidth();
    if (!entry.hasBorderWidth || !areScalarsEqual(entry.cachedBorderWidth, borderWidth)) {
        borderProp->SetLineWidth(static_cast<float>(borderWidth));
        entry.cachedBorderWidth = borderWidth;
        entry.hasBorderWidth = true;
    }

    const int newLayer = getNodeLayerInWindow(node);
    if (newLayer != entry.currentLayer) {
        vtkRenderer* oldRenderer = getRenderer(entry.currentLayer);
        vtkRenderer* newRenderer = getRenderer(newLayer);
        if (oldRenderer) {
            oldRenderer->RemoveActor(entry.fillActor);
            oldRenderer->RemoveActor(entry.borderActor);
        }
        if (newRenderer) {
            newRenderer->AddActor(entry.fillActor);
            newRenderer->AddActor(entry.borderActor);
        }
        entry.currentLayer = newLayer;
    }

    const bool nodeVisible = isNodeVisibleInWindow(node);
    const bool fillVisible = nodeVisible && fillOpacity > kTolerance;
    const bool borderVisible = nodeVisible && borderOpacity > kTolerance && borderWidth > kTolerance;
    if (entry.fillVisible != fillVisible) {
        entry.fillActor->SetVisibility(fillVisible ? 1 : 0);
        entry.fillVisible = fillVisible;
    }
    if (entry.borderVisible != borderVisible) {
        entry.borderActor->SetVisibility(borderVisible ? 1 : 0);
        entry.borderVisible = borderVisible;
    }
}

void PlaneNodeDisplayManager::updateTransform(const QString& nodeId)
{
    auto it = m_entries.find(nodeId);
    if (it == m_entries.end()) {
        return;
    }

    PlaneDisplayEntry& entry = it.value();
    double matrix[16];
    if (!scene()->getWorldTransformMatrix(nodeId, matrix)) {
        if (entry.hasWorldTransform) {
            entry.fillActor->SetUserTransform(nullptr);
            entry.borderActor->SetUserTransform(nullptr);
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
    entry.fillActor->SetUserTransform(entry.transform);
    entry.borderActor->SetUserTransform(entry.transform);
    copyArray(matrix, entry.cachedWorldMatrix, 16);
    entry.hasWorldTransform = true;
}