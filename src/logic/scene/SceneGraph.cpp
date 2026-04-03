#include "SceneGraph.h"

#include <QReadLocker>
#include <QWriteLocker>

namespace {

void setIdentityMatrix(double out[16])
{
    for (int index = 0; index < 16; ++index) {
        out[index] = 0.0;
    }
    out[0] = 1.0;
    out[5] = 1.0;
    out[10] = 1.0;
    out[15] = 1.0;
}

void multiplyColumnMajor(const double left[16], const double right[16], double out[16])
{
    double result[16] = {0.0};
    for (int column = 0; column < 4; ++column) {
        for (int row = 0; row < 4; ++row) {
            double value = 0.0;
            for (int term = 0; term < 4; ++term) {
                value += left[term * 4 + row] * right[column * 4 + term];
            }
            result[column * 4 + row] = value;
        }
    }

    for (int index = 0; index < 16; ++index) {
        out[index] = result[index];
    }
}

QString parentTransformIdForNode(const NodeBase* node)
{
    if (!node) {
        return QString();
    }

    return node->getFirstReference(NodeBase::parentTransformReferenceRole());
}

bool resolveTransformWorldMatrixLocked(const QMap<QString, NodeBase*>& nodes,
                                       const QString& transformId,
                                       double out[16],
                                       QSet<QString>& visited)
{
    auto* transformNode = dynamic_cast<TransformNode*>(nodes.value(transformId, nullptr));
    if (!transformNode) {
        return false;
    }

    if (visited.contains(transformId)) {
        return false;
    }
    visited.insert(transformId);

    double localMatrix[16];
    transformNode->getMatrixTransformToParent(localMatrix);

    const QString parentTransformId = parentTransformIdForNode(transformNode);
    if (parentTransformId.isEmpty()) {
        for (int index = 0; index < 16; ++index) {
            out[index] = localMatrix[index];
        }
        return true;
    }

    double parentWorld[16];
    if (!resolveTransformWorldMatrixLocked(nodes, parentTransformId, parentWorld, visited)) {
        return false;
    }

    multiplyColumnMajor(parentWorld, localMatrix, out);
    return true;
}

} // namespace

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

NodeBase* SceneGraph::getNodeByName(const QString& name) const
{
    QReadLocker locker(&m_lock);
    for (NodeBase* node : m_nodes) {
        if (node && node->getName() == name) {
            return node;
        }
    }
    return nullptr;
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

bool SceneGraph::canAssignParentTransform(const QString& nodeId, const QString& transformId) const
{
    if (nodeId.isEmpty()) {
        return false;
    }

    if (transformId.isEmpty()) {
        return true;
    }

    QReadLocker locker(&m_lock);

    if (nodeId == transformId) {
        return false;
    }

    auto* referencedTransform = dynamic_cast<TransformNode*>(m_nodes.value(transformId, nullptr));
    if (!referencedTransform) {
        return false;
    }

    auto* node = m_nodes.value(nodeId, nullptr);
    if (!node) {
        return true;
    }

    if (!dynamic_cast<TransformNode*>(node)) {
        return true;
    }

    QString currentId = transformId;
    QSet<QString> visited;
    visited.insert(nodeId);

    while (!currentId.isEmpty()) {
        if (visited.contains(currentId)) {
            return false;
        }

        visited.insert(currentId);
        auto* currentTransform = dynamic_cast<TransformNode*>(m_nodes.value(currentId, nullptr));
        if (!currentTransform) {
            return false;
        }

        currentId = parentTransformIdForNode(currentTransform);
    }

    return true;
}

bool SceneGraph::getWorldTransformMatrix(const QString& nodeId, double out[16]) const
{
    if (!out) {
        return false;
    }

    QReadLocker locker(&m_lock);
    NodeBase* node = m_nodes.value(nodeId, nullptr);
    if (!node) {
        return false;
    }

    if (dynamic_cast<TransformNode*>(node)) {
        QSet<QString> visited;
        return resolveTransformWorldMatrixLocked(m_nodes, nodeId, out, visited);
    }

    const QString parentTransformId = parentTransformIdForNode(node);
    if (parentTransformId.isEmpty()) {
        return false;
    }

    QSet<QString> visited;
    return resolveTransformWorldMatrixLocked(m_nodes, parentTransformId, out, visited);
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
