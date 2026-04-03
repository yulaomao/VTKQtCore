#pragma once

#include "NodeDisplayManager.h"

#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkCellArray.h>
#include <vtkMatrix4x4.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkTransform.h>

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
        vtkSmartPointer<vtkActor> actor;
        vtkSmartPointer<vtkPolyData> polyData;
        vtkSmartPointer<vtkPoints> points;
        vtkSmartPointer<vtkCellArray> cells;
        vtkSmartPointer<vtkPolyDataMapper> mapper;
        vtkSmartPointer<vtkTransform> transform;
        vtkSmartPointer<vtkMatrix4x4> transformMatrix;
        int currentLayer = 3;
        bool actorVisible = false;
        bool dashed = false;
        bool hasColor = false;
        bool hasOpacity = false;
        bool hasLineWidth = false;
        bool hasWorldTransform = false;
        double cachedColor[4] = {0.0, 0.0, 0.0, 0.0};
        double cachedOpacity = 1.0;
        double cachedLineWidth = 1.0;
        double cachedWorldMatrix[16] = {0.0};
    };

    void buildEntry(const QString& nodeId);
    void removeEntry(const QString& nodeId);
    void updateContent(const QString& nodeId);
    void updateDisplay(const QString& nodeId);
    void updateTransform(const QString& nodeId);

    QMap<QString, LineDisplayEntry> m_entries;
};
