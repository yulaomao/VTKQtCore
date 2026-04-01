#pragma once

#include "NodeDisplayManager.h"

#include <vtkSmartPointer.h>
#include <vtkActor.h>

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
        int currentLayer = 3;
    };

    void buildEntry(const QString& nodeId);
    void removeEntry(const QString& nodeId);
    void updateContent(const QString& nodeId);
    void updateDisplay(const QString& nodeId);
    void updateTransform(const QString& nodeId);

    vtkSmartPointer<vtkPolyData> buildPolyLine(class LineNode* node) const;

    QMap<QString, LineDisplayEntry> m_entries;
};
