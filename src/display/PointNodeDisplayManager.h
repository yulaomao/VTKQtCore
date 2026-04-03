#pragma once

#include "NodeDisplayManager.h"

#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkActor2D.h>
#include <vtkCellArray.h>
#include <vtkGlyph3DMapper.h>
#include <vtkLabeledDataMapper.h>
#include <vtkMatrix4x4.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSphereSource.h>
#include <vtkStringArray.h>
#include <vtkTransform.h>

#include <QMap>
#include <QString>

class PointNodeDisplayManager : public NodeDisplayManager
{
    Q_OBJECT

public:
    PointNodeDisplayManager(SceneGraph* scene, const QString& windowId,
                            vtkRenderer* layer1, vtkRenderer* layer2, vtkRenderer* layer3,
                            QObject* parent = nullptr);
    ~PointNodeDisplayManager() override;

    void onNodeAdded(const QString& nodeId) override;
    void onNodeRemoved(const QString& nodeId) override;
    void onNodeModified(const QString& nodeId, NodeEventType eventType) override;
    void reconcileWithScene() override;
    void clearAll() override;
    bool canHandleNode(NodeBase* node) const override;

private:
    struct PointDisplayEntry {
        vtkSmartPointer<vtkActor> actor;
        vtkSmartPointer<vtkActor2D> labelActor;
        vtkSmartPointer<vtkPolyData> polyData;
        vtkSmartPointer<vtkPoints> points;
        vtkSmartPointer<vtkCellArray> verts;
        vtkSmartPointer<vtkStringArray> labels;
        vtkSmartPointer<vtkSphereSource> sphere;
        vtkSmartPointer<vtkGlyph3DMapper> glyphMapper;
        vtkSmartPointer<vtkLabeledDataMapper> labelMapper;
        vtkSmartPointer<vtkTransform> transform;
        vtkSmartPointer<vtkMatrix4x4> transformMatrix;
        int currentLayer = 3;
        bool actorVisible = false;
        bool labelVisible = false;
        bool hasRadius = false;
        bool hasDisplayColor = false;
        bool hasWorldTransform = false;
        double cachedRadius = 0.0;
        double cachedDisplayColor[4] = {0.0, 0.0, 0.0, 0.0};
        double cachedWorldMatrix[16] = {0.0};
    };

    void buildEntry(const QString& nodeId);
    void removeEntry(const QString& nodeId);
    void updateContent(const QString& nodeId);
    void updateDisplay(const QString& nodeId);
    void updateTransform(const QString& nodeId);

    QMap<QString, PointDisplayEntry> m_entries;
};
