#pragma once

#include "NodeDisplayManager.h"

#include <vtkSmartPointer.h>
#include <vtkActor2D.h>
#include <vtkCellArray.h>
#include <vtkCoordinate.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper2D.h>

#include <QMap>
#include <QString>

class BillboardLineNodeDisplayManager : public NodeDisplayManager
{
    Q_OBJECT

public:
    BillboardLineNodeDisplayManager(SceneGraph* scene, const QString& windowId,
                                    vtkRenderer* layer1, vtkRenderer* layer2, vtkRenderer* layer3,
                                    QObject* parent = nullptr);
    ~BillboardLineNodeDisplayManager() override;

    void onNodeAdded(const QString& nodeId) override;
    void onNodeRemoved(const QString& nodeId) override;
    void onNodeModified(const QString& nodeId, NodeEventType eventType) override;
    void reconcileWithScene() override;
    void clearAll() override;
    bool canHandleNode(NodeBase* node) const override;

private:
    struct LineDisplayEntry {
        vtkSmartPointer<vtkActor2D> actor;
        vtkSmartPointer<vtkPolyData> polyData;
        vtkSmartPointer<vtkPoints> points;
        vtkSmartPointer<vtkCellArray> cells;
        vtkSmartPointer<vtkCoordinate> coordinate;
        vtkSmartPointer<vtkPolyDataMapper2D> mapper;
        int currentLayer = 3;
    };

    void buildEntry(const QString& nodeId);
    void removeEntry(const QString& nodeId);
    void updateContent(const QString& nodeId);
    void updateDisplay(const QString& nodeId);

    QMap<QString, LineDisplayEntry> m_entries;
};