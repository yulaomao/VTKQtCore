#pragma once

#include "NodeDisplayManager.h"

#include <vtkCellArray.h>
#include <vtkGlyph3DMapper.h>
#include <vtkLabeledDataMapper.h>
#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkActor2D.h>
#include <vtkMatrix4x4.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkSphereSource.h>
#include <vtkStringArray.h>
#include <vtkTransform.h>

#include <array>
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
        vtkSmartPointer<vtkPoints> points;
        vtkSmartPointer<vtkCellArray> verts;
        vtkSmartPointer<vtkStringArray> labels;
        vtkSmartPointer<vtkPolyData> polyData;
        vtkSmartPointer<vtkSphereSource> sphere;
        vtkSmartPointer<vtkGlyph3DMapper> glyphMapper;
        vtkSmartPointer<vtkLabeledDataMapper> labelMapper;
        vtkSmartPointer<vtkActor> actor;
        vtkSmartPointer<vtkActor2D> labelActor;
        vtkSmartPointer<vtkTransform> transform;
        vtkSmartPointer<vtkMatrix4x4> transformMatrix;
        std::array<double, 4> color = {0.0, 0.0, 0.0, 1.0};
        std::array<double, 16> transformValues = {
            0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0
        };
        double sphereRadius = 0.0;
        bool visible = true;
        bool labelsVisible = false;
        bool hasTransform = false;
        int currentLayer = 3;
    };

    void buildEntry(const QString& nodeId);
    void removeEntry(const QString& nodeId);
    void updateContent(const QString& nodeId);
    void updateDisplay(const QString& nodeId);
    void updateTransform(const QString& nodeId);
    void populatePointData(PointNode* node,
                           vtkPoints* points,
                           vtkCellArray* verts,
                           vtkStringArray* labels,
                           vtkPolyData* polyData) const;

    QMap<QString, PointDisplayEntry> m_entries;
};
