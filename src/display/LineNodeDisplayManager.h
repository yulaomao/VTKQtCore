#pragma once

#include "NodeDisplayManager.h"

#include <vtkCellArray.h>
#include <vtkMatrix4x4.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkTransform.h>

#include <array>
#include <QMap>
#include <QString>

class LineNodeDisplayManager : public NodeDisplayManager
{
    Q_OBJECT

public:
    LineNodeDisplayManager(SceneGraph* scene, const QString& windowId,
                           vtkRenderer* layer1, vtkRenderer* layer2, vtkRenderer* layer3,
                           QObject* parent = nullptr);
    ~LineNodeDisplayManager() override;

    void onNodeAdded(const QString& nodeId) override;
    void onNodeRemoved(const QString& nodeId) override;
    void onNodeModified(const QString& nodeId, NodeEventType eventType) override;
    void reconcileWithScene() override;
    void clearAll() override;
    bool canHandleNode(NodeBase* node) const override;

private:
    struct LineDisplayEntry {
        vtkSmartPointer<vtkPoints> points;
        vtkSmartPointer<vtkCellArray> cells;
        vtkSmartPointer<vtkPolyData> polyData;
        vtkSmartPointer<vtkPolyDataMapper> mapper;
        vtkSmartPointer<vtkActor> actor;
        vtkSmartPointer<vtkTransform> transform;
        vtkSmartPointer<vtkMatrix4x4> transformMatrix;
        std::array<double, 4> color = {0.0, 0.0, 0.0, 1.0};
        std::array<double, 16> transformValues = {
            0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0,
            0.0, 0.0, 0.0, 0.0
        };
        double opacity = 1.0;
        double lineWidth = 1.0;
        bool dashed = false;
        bool visible = true;
        bool hasTransform = false;
        int currentLayer = 3;
    };

    void buildEntry(const QString& nodeId);
    void removeEntry(const QString& nodeId);
    void updateContent(const QString& nodeId);
    void updateDisplay(const QString& nodeId);
    void updateTransform(const QString& nodeId);

    void populatePolyLineData(class LineNode* node,
                              vtkPoints* points,
                              vtkCellArray* cells,
                              vtkPolyData* polyData) const;

    QMap<QString, LineDisplayEntry> m_entries;
};
