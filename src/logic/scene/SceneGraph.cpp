#include "SceneGraph.h"

#include <QReadLocker>
#include <QWriteLocker>

SceneGraph::SceneGraph(QObject* parent)
    : QObject(parent)
{
}

SceneGraph::~SceneGraph()
{
    removeAllNodes();
}

void SceneGraph::addNode(NodeBase* node)
{
    if (!node) {
        return;
    }

    const QString nodeId = node->getNodeId();
    if (nodeId.isEmpty()) {
        return;
    }

    {
        QWriteLocker locker(&m_lock);
        if (m_nodes.contains(nodeId)) {
            return;
        }
        node->setParent(this);
        m_nodes.insert(nodeId, node);
    }

    connect(node, &NodeBase::nodeEvent, this, &SceneGraph::onNodeEvent);
    emit nodeAdded(nodeId);
}

bool SceneGraph::removeNode(const QString& nodeId)
{
    NodeBase* node = nullptr;

    {
        QWriteLocker locker(&m_lock);
        auto it = m_nodes.find(nodeId);
        if (it == m_nodes.end()) {
            return false;
        }
        node = it.value();
        m_nodes.erase(it);
    }

    disconnect(node, &NodeBase::nodeEvent, this, &SceneGraph::onNodeEvent);
    delete node;
    emit nodeRemoved(nodeId);
    return true;
}

void SceneGraph::removeAllNodes()
{
    QList<QString> ids;

    {
        QWriteLocker locker(&m_lock);
        ids = m_nodes.keys();
    }

    for (const QString& id : ids) {
        removeNode(id);
    }
}

NodeBase* SceneGraph::getNodeById(const QString& nodeId) const
{
    QReadLocker locker(&m_lock);
    return m_nodes.value(nodeId, nullptr);
}

QVector<NodeBase*> SceneGraph::getNodesByTagName(const QString& tagName) const
{
    QReadLocker locker(&m_lock);
    QVector<NodeBase*> result;
    for (NodeBase* node : m_nodes) {
        if (node->getNodeTagName() == tagName) {
            result.append(node);
        }
    }
    return result;
}

QVector<NodeBase*> SceneGraph::getNodesByAttribute(const QString& key, const QVariant& value) const
{
    QReadLocker locker(&m_lock);
    QVector<NodeBase*> result;
    for (NodeBase* node : m_nodes) {
        if (node->getAttribute(key) == value) {
            result.append(node);
        }
    }
    return result;
}

QVector<NodeBase*> SceneGraph::getAllNodes() const
{
    QReadLocker locker(&m_lock);
    QVector<NodeBase*> result;
    result.reserve(m_nodes.size());
    for (NodeBase* node : m_nodes) {
        result.append(node);
    }
    return result;
}

int SceneGraph::getNodeCount() const
{
    QReadLocker locker(&m_lock);
    return m_nodes.size();
}

bool SceneGraph::containsNode(const QString& nodeId) const
{
    QReadLocker locker(&m_lock);
    return m_nodes.contains(nodeId);
}

void SceneGraph::startBatchModify()
{
    QWriteLocker locker(&m_lock);
    ++m_batchDepth;
}

void SceneGraph::endBatchModify()
{
    bool shouldEmit = false;

    {
        QWriteLocker locker(&m_lock);
        if (m_batchDepth > 0) {
            --m_batchDepth;
            if (m_batchDepth == 0) {
                shouldEmit = true;
            }
        }
    }

    if (shouldEmit) {
        emit batchModifyEnded();
    }
}

QVector<PointNode*> SceneGraph::getAllPointNodes() const
{
    QReadLocker locker(&m_lock);
    QVector<PointNode*> result;
    for (NodeBase* node : m_nodes) {
        if (auto* typed = dynamic_cast<PointNode*>(node)) {
            result.append(typed);
        }
    }
    return result;
}

QVector<LineNode*> SceneGraph::getAllLineNodes() const
{
    QReadLocker locker(&m_lock);
    QVector<LineNode*> result;
    for (NodeBase* node : m_nodes) {
        if (auto* typed = dynamic_cast<LineNode*>(node)) {
            result.append(typed);
        }
    }
    return result;
}

QVector<ModelNode*> SceneGraph::getAllModelNodes() const
{
    QReadLocker locker(&m_lock);
    QVector<ModelNode*> result;
    for (NodeBase* node : m_nodes) {
        if (auto* typed = dynamic_cast<ModelNode*>(node)) {
            result.append(typed);
        }
    }
    return result;
}

QVector<TransformNode*> SceneGraph::getAllTransformNodes() const
{
    QReadLocker locker(&m_lock);
    QVector<TransformNode*> result;
    for (NodeBase* node : m_nodes) {
        if (auto* typed = dynamic_cast<TransformNode*>(node)) {
            result.append(typed);
        }
    }
    return result;
}

void SceneGraph::onNodeEvent(const QString& nodeId, NodeEventType eventType)
{
    QReadLocker locker(&m_lock);
    if (m_batchDepth == 0) {
        locker.unlock();
        emit nodeModified(nodeId, eventType);
    }
}
