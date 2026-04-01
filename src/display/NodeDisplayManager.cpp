#include "NodeDisplayManager.h"

#include <vtkRenderer.h>

NodeDisplayManager::NodeDisplayManager(SceneGraph* scene, const QString& windowId,
                                       vtkRenderer* layer1, vtkRenderer* layer2,
                                       vtkRenderer* layer3, QObject* parent)
    : QObject(parent)
    , m_sceneGraph(scene)
    , m_windowId(windowId)
    , m_renderers{layer1, layer2, layer3}
{
    connect(m_sceneGraph, &SceneGraph::nodeAdded,
            this, &NodeDisplayManager::handleNodeAdded);
    connect(m_sceneGraph, &SceneGraph::nodeRemoved,
            this, &NodeDisplayManager::handleNodeRemoved);
    connect(m_sceneGraph, &SceneGraph::nodeModified,
            this, &NodeDisplayManager::handleNodeModified);
    connect(m_sceneGraph, &SceneGraph::batchModifyEnded,
            this, &NodeDisplayManager::handleBatchModifyEnded);
}

void NodeDisplayManager::handleNodeAdded(const QString& nodeId)
{
    NodeBase* node = m_sceneGraph->getNodeById(nodeId);
    if (node && canHandleNode(node)) {
        onNodeAdded(nodeId);
    }
}

void NodeDisplayManager::handleNodeRemoved(const QString& nodeId)
{
    onNodeRemoved(nodeId);
}

void NodeDisplayManager::handleNodeModified(const QString& nodeId, NodeEventType eventType)
{
    NodeBase* node = m_sceneGraph->getNodeById(nodeId);
    if (node && canHandleNode(node)) {
        onNodeModified(nodeId, eventType);
    }
}

void NodeDisplayManager::handleBatchModifyEnded()
{
    reconcileWithScene();
}

SceneGraph* NodeDisplayManager::scene() const
{
    return m_sceneGraph;
}

const QString& NodeDisplayManager::windowId() const
{
    return m_windowId;
}

vtkRenderer* NodeDisplayManager::getRenderer(int layer) const
{
    if (layer >= 1 && layer <= 3) {
        return m_renderers[layer - 1];
    }
    return nullptr;
}

bool NodeDisplayManager::isNodeVisibleInWindow(NodeBase* node) const
{
    if (!node) {
        return false;
    }
    DisplayTarget target = node->getDisplayTargetForWindow(m_windowId);
    return target.visible;
}

int NodeDisplayManager::getNodeLayerInWindow(NodeBase* node) const
{
    if (!node) {
        return 1;
    }
    DisplayTarget target = node->getDisplayTargetForWindow(m_windowId);
    return target.layer;
}
