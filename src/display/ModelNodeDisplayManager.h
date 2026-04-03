#pragma once

#include "NodeDisplayManager.h"

#include <vtkMatrix4x4.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkTransform.h>

#include <array>
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
        vtkSmartPointer<vtkPolyData> polyData;
        vtkSmartPointer<vtkPolyDataMapper> mapper;
        vtkSmartPointer<vtkActor> actor;
        vtkSmartPointer<vtkTransform> transform;
        vtkSmartPointer<vtkMatrix4x4> transformMatrix;
        std::array<double, 4> color = {0.0, 0.0, 0.0, 1.0};
        std::array<double, 4> edgeColor = {0.0, 0.0, 0.0, 1.0};
        std::array<double, 16> transformValues = {
            0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0
        };
        QString renderMode;
        double opacity = 1.0;
        double edgeWidth = 1.0;
        bool visible = true;
        bool scalarVisibility = false;
        bool backfaceCulling = false;
        bool edgeVisibility = false;
        bool hasTransform = false;
        int currentLayer = 1;
    };

    void buildEntry(const QString& nodeId);
    void removeEntry(const QString& nodeId);
    void updateContent(const QString& nodeId);
    void updateDisplay(const QString& nodeId);
    void updateTransform(const QString& nodeId);

    void applyVisualProperties(ModelDisplayEntry& entry, class ModelNode* node);

    QMap<QString, ModelDisplayEntry> m_entries;
};
