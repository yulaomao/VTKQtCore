#pragma once

#include "../logic/scene/SceneGraph.h"
#include "../logic/scene/nodes/NodeBase.h"

#include <QObject>
#include <QString>

class vtkRenderer;

class NodeDisplayManager : public QObject
{
    Q_OBJECT

public:
    NodeDisplayManager(SceneGraph* scene, const QString& windowId,
                       vtkRenderer* layer1, vtkRenderer* layer2, vtkRenderer* layer3,
                       QObject* parent = nullptr);
    ~NodeDisplayManager() override = default;

    virtual void onNodeAdded(const QString& nodeId) = 0;
    virtual void onNodeRemoved(const QString& nodeId) = 0;
    virtual void onNodeModified(const QString& nodeId, NodeEventType eventType) = 0;
    virtual void reconcileWithScene() = 0;
    virtual void clearAll() = 0;

    virtual bool canHandleNode(NodeBase* node) const = 0;

public slots:
    void handleNodeAdded(const QString& nodeId);
    void handleNodeRemoved(const QString& nodeId);
    void handleNodeModified(const QString& nodeId, NodeEventType eventType);
    void handleBatchModifyEnded();

protected:
    SceneGraph* scene() const;
    const QString& windowId() const;
    vtkRenderer* getRenderer(int layer) const;

    bool isNodeVisibleInWindow(NodeBase* node) const;
    int getNodeLayerInWindow(NodeBase* node) const;

private:
    SceneGraph* m_sceneGraph;
    QString m_windowId;
    vtkRenderer* m_renderers[3];
};
