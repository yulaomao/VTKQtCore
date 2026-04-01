#pragma once

#include "nodes/NodeBase.h"
#include "nodes/PointNode.h"
#include "nodes/LineNode.h"
#include "nodes/ModelNode.h"
#include "nodes/TransformNode.h"

#include <QObject>
#include <QMap>
#include <QVector>
#include <QReadWriteLock>

class SceneGraph : public QObject
{
    Q_OBJECT

public:
    explicit SceneGraph(QObject* parent = nullptr);
    ~SceneGraph() override;

    // Add/Remove
    void addNode(NodeBase* node);
    bool removeNode(const QString& nodeId);
    void removeAllNodes();

    // Query
    NodeBase* getNodeById(const QString& nodeId) const;

    template<typename T>
    T* getNodeById(const QString& nodeId) const
    {
        return dynamic_cast<T*>(getNodeById(nodeId));
    }

    QVector<NodeBase*> getNodesByTagName(const QString& tagName) const;
    QVector<NodeBase*> getNodesByAttribute(const QString& key, const QVariant& value) const;
    QVector<NodeBase*> getAllNodes() const;
    int getNodeCount() const;
    bool containsNode(const QString& nodeId) const;
    bool canAssignParentTransform(const QString& nodeId, const QString& transformId) const;
    bool getWorldTransformMatrix(const QString& nodeId, double out[16]) const;

    // Batch modify (scene level)
    void startBatchModify();
    void endBatchModify();

    // Convenience typed queries
    QVector<PointNode*> getAllPointNodes() const;
    QVector<LineNode*> getAllLineNodes() const;
    QVector<ModelNode*> getAllModelNodes() const;
    QVector<TransformNode*> getAllTransformNodes() const;

signals:
    void nodeAdded(const QString& nodeId);
    void nodeRemoved(const QString& nodeId);
    void nodeModified(const QString& nodeId, NodeEventType eventType);
    void batchModifyEnded();

private slots:
    void onNodeEvent(const QString& nodeId, NodeEventType eventType);

private:
    QMap<QString, NodeBase*> m_nodes;
    int m_batchDepth = 0;
    mutable QReadWriteLock m_lock;
};
