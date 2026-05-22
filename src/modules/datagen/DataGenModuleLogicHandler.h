#pragma once

#include "logic/registry/ModuleLogicHandler.h"

#include <QString>
#include <QVariantList>

class QTimer;

class NodeBase;
class PointNode;
class LineNode;
class ModelNode;
class PlaneNode;
class TransformNode;
class SceneGraph;

class DataGenModuleLogicHandler : public ModuleLogicHandler
{
    Q_OBJECT

public:
    explicit DataGenModuleLogicHandler(QObject* parent = nullptr);

    void handleAction(const UiAction& action) override;
    ModuleInvokeResult handleModuleInvoke(const ModuleInvokeRequest& request) override;
    void onModuleActivated() override;
    void onResync() override;

private:
    static QString moduleOwnerTag();

    void ensureSeedScene();
    void handleCustomCommand(const QVariantMap& payload, const QString& sourceActionId);
    void playPromptPresetBurst(const QString& presetId, int count, int intervalMs,
                               const QString& sourceActionId = QString());
    void emitState(const QString& statusText,
                   LogicNotification::EventType eventType = LogicNotification::SceneNodesUpdated,
                   const QString& sourceActionId = QString());
    QVariantMap buildState(const QString& statusText) const;
    QVariantList buildNodeSummaries() const;
    QVariantList buildTransformOptions() const;
    QVariantMap buildNodeDetails(NodeBase* node) const;
    QVector<NodeBase*> managedNodes() const;
    QVector<TransformNode*> managedTransforms() const;
    NodeBase* nodeById(const QString& nodeId) const;
    TransformNode* transformById(const QString& nodeId) const;
    void setManagedDefaults(NodeBase* node, int layer) const;
    void removeParentReferencesTo(const QString& nodeId);
    void selectFallbackNode();

    PointNode* createPointNode(const QVariantMap& payload);
    LineNode* createLineNode(const QVariantMap& payload);
    ModelNode* createModelNode(const QVariantMap& payload);
    PlaneNode* createPlaneNode(const QVariantMap& payload);
    TransformNode* createTransformNode(const QVariantMap& payload);

    void updateDisplay(NodeBase* node, const QVariantMap& payload);
    void assignParent(NodeBase* node, const QString& parentTransformId);
    void updateTransformPose(TransformNode* node, const QVariantMap& payload);
    void clearNodeGeometry(NodeBase* node);
    bool deleteNode(const QString& nodeId);
    void stopPromptBurst();

    QString m_selectedNodeId;
    QString m_statusText = QStringLiteral("数据生成模块已就绪。");
    QTimer* m_promptBurstTimer = nullptr;
    QString m_promptBurstPresetId;
    QString m_promptBurstSourceActionId;
    int m_promptBurstRemaining = 0;
};