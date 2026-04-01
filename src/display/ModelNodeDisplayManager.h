#pragma once

#include "NodeDisplayManager.h"

#include <vtkSmartPointer.h>
#include <vtkActor.h>

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
        vtkSmartPointer<vtkActor> actor;
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
