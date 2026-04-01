#pragma once

#include "NodeDisplayManager.h"

#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkActor2D.h>

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
        int currentLayer = 3;
    };

    void buildEntry(const QString& nodeId);
    void removeEntry(const QString& nodeId);
    void updateContent(const QString& nodeId);
    void updateDisplay(const QString& nodeId);
    void updateTransform(const QString& nodeId);

    QMap<QString, PointDisplayEntry> m_entries;
};
