#pragma once

#include "NodeDisplayManager.h"

#include <vtkSmartPointer.h>
#include <vtkActor.h>

#include <QMap>
#include <QString>

class TransformNodeDisplayManager : public NodeDisplayManager
{
    Q_OBJECT

public:
    TransformNodeDisplayManager(SceneGraph* scene, const QString& windowId,
                                vtkRenderer* layer1, vtkRenderer* layer2, vtkRenderer* layer3,
                                QObject* parent = nullptr);
    ~TransformNodeDisplayManager() override;

    void onNodeAdded(const QString& nodeId) override;
    void onNodeRemoved(const QString& nodeId) override;
    void onNodeModified(const QString& nodeId, NodeEventType eventType) override;
    void reconcileWithScene() override;
    void clearAll() override;
    bool canHandleNode(NodeBase* node) const override;

private:
    struct TransformDisplayEntry {
        vtkSmartPointer<vtkActor> xAxisActor;
        vtkSmartPointer<vtkActor> yAxisActor;
        vtkSmartPointer<vtkActor> zAxisActor;
        int currentLayer = 3;
    };

    void buildEntry(const QString& nodeId);
    void removeEntry(const QString& nodeId);
    void updateAxes(const QString& nodeId);
    void updateDisplay(const QString& nodeId);

    void computeAxisEndpoint(const double matrix[16], int axisIndex,
                             double length, double out[3]) const;

    QMap<QString, TransformDisplayEntry> m_entries;
};
