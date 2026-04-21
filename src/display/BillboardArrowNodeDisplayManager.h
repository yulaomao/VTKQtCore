#pragma once

#include "NodeDisplayManager.h"

#include <vtkSmartPointer.h>
#include <vtkActor2D.h>
#include <vtkCallbackCommand.h>
#include <vtkCellArray.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper2D.h>

#include <QMap>
#include <QString>

class vtkRenderer;

class BillboardArrowNodeDisplayManager : public NodeDisplayManager
{
    Q_OBJECT

public:
    BillboardArrowNodeDisplayManager(SceneGraph* scene, const QString& windowId,
                                     vtkRenderer* layer1, vtkRenderer* layer2, vtkRenderer* layer3,
                                     QObject* parent = nullptr);
    ~BillboardArrowNodeDisplayManager() override;

    void onNodeAdded(const QString& nodeId) override;
    void onNodeRemoved(const QString& nodeId) override;
    void onNodeModified(const QString& nodeId, NodeEventType eventType) override;
    void reconcileWithScene() override;
    void clearAll() override;
    bool canHandleNode(NodeBase* node) const override;

private:
    void detachRendererObservers();

    struct ArrowDisplayEntry {
        vtkSmartPointer<vtkActor2D> actor;
        vtkSmartPointer<vtkPolyData> polyData;
        vtkSmartPointer<vtkPoints> points;
        vtkSmartPointer<vtkCellArray> polygons;
        vtkSmartPointer<vtkCellArray> lines;
        vtkSmartPointer<vtkPolyDataMapper2D> mapper;
        int currentLayer = 3;
        bool hasCapturedWorldDirection = false;
        double capturedWorldDirection[3] = {0.0, 0.0, 0.0};
        QString cachedDirection = QStringLiteral("up");
        bool cachedFollowCameraRotation = false;
    };

    void buildEntry(const QString& nodeId);
    void removeEntry(const QString& nodeId);
    void updateDisplay(const QString& nodeId);
    void updateDisplayPosition(const QString& nodeId, vtkRenderer* renderer);
    void onRendererStart(vtkRenderer* renderer);

    QMap<QString, ArrowDisplayEntry> m_entries;
    vtkSmartPointer<vtkCallbackCommand> m_renderCallback;
    unsigned long m_rendererObserverTags[3] = {0, 0, 0};
};