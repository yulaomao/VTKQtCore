#pragma once

#include "NodeDisplayManager.h"

#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkMatrix4x4.h>
#include <vtkPolyData.h>
#include <vtkTransform.h>

#include <QMap>
#include <QString>

class ModelNodeDisplayManager : public NodeDisplayManager
{
    Q_OBJECT

public:
    ModelNodeDisplayManager(SceneGraph* scene, const QString& windowId,
                            vtkRenderer* layer1, vtkRenderer* layer2, vtkRenderer* layer3,
                            QObject* parent = nullptr);
    ~ModelNodeDisplayManager() override;

    void onNodeAdded(const QString& nodeId) override;
    void onNodeRemoved(const QString& nodeId) override;
    void onNodeModified(const QString& nodeId, NodeEventType eventType) override;
    void reconcileWithScene() override;
    void clearAll() override;
    bool canHandleNode(NodeBase* node) const override;

private:
    struct ModelDisplayEntry {
        vtkSmartPointer<vtkActor> actor;
        vtkSmartPointer<vtkPolyData> currentPolyData;
        vtkSmartPointer<vtkTransform> transform;
        vtkSmartPointer<vtkMatrix4x4> transformMatrix;
        int currentLayer = 1;
        bool actorVisible = false;
        bool hasRenderMode = false;
        bool hasColor = false;
        bool hasOpacity = false;
        bool hasBackfaceCulling = false;
        bool hasShowEdges = false;
        bool hasEdgeColor = false;
        bool hasEdgeWidth = false;
        bool hasScalarVisibility = false;
        bool hasWorldTransform = false;
        QString cachedRenderMode;
        double cachedColor[4] = {0.0, 0.0, 0.0, 0.0};
        double cachedOpacity = 1.0;
        bool cachedBackfaceCulling = false;
        bool cachedShowEdges = false;
        double cachedEdgeColor[4] = {0.0, 0.0, 0.0, 0.0};
        double cachedEdgeWidth = 1.0;
        bool cachedScalarVisibility = false;
        double cachedWorldMatrix[16] = {0.0};
    };

    void buildEntry(const QString& nodeId);
    void removeEntry(const QString& nodeId);
    void updateContent(const QString& nodeId);
    void updateDisplay(const QString& nodeId);
    void updateTransform(const QString& nodeId);

    void applyVisualProperties(ModelDisplayEntry& entry, class ModelNode* node);

    QMap<QString, ModelDisplayEntry> m_entries;
};
