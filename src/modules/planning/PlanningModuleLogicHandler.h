#pragma once

#include "logic/registry/ModuleLogicHandler.h"
#include <QString>
#include <QStringList>
#include <QVector>

#include <array>

class SceneGraph;
class BillboardArrowNode;
class BillboardLineNode;
class ModelNode;

class PlanningModuleLogicHandler : public ModuleLogicHandler
{
    Q_OBJECT

public:
    explicit PlanningModuleLogicHandler(QObject* parent = nullptr);

    void handleAction(const UiAction& action) override;
    void handlePollData(const QString& key, const QVariant& value) override;
    void handleSubscribeData(const QString& channel, const QVariantMap& data) override;
    void onModuleActivated() override;
    void onModuleDeactivated() override;
    void onResync() override;

private:
    void handleIncomingData(const QVariantMap& payloadData);
    ModelNode* ensurePlanModelNode(SceneGraph* scene);
    BillboardArrowNode* ensurePlanPathArrowNode(SceneGraph* scene);
    QVector<BillboardLineNode*> ensurePlanPathSegmentNodes(SceneGraph* scene, int segmentCount);
    ModelNode* findPlanModelNode(SceneGraph* scene) const;
    BillboardArrowNode* findPlanPathArrowNode(SceneGraph* scene) const;
    QVector<BillboardLineNode*> findPlanPathSegmentNodes(SceneGraph* scene) const;
    void restorePlanPathVisualizationNodes(SceneGraph* scene);
    void syncPlanPathVisualization(SceneGraph* scene,
                                   const QVector<std::array<double, 3>>& pathPoints);
    void removeLegacyPlanPathNodes(SceneGraph* scene) const;
    void removePlanPathArrowNode(SceneGraph* scene);
    void ensureDefaultPlanGeometry(SceneGraph* scene, ModelNode* modelNode);
    void emitPlanningState(LogicNotification::EventType eventType,
                           const QString& sourceActionId = QString(),
                           const QString& sourceSampleId = QString());

    QString m_planModelNodeId;
    QStringList m_planPathSegmentNodeIds;
    QString m_planPathArrowNodeId;
    QString m_planStatus = QStringLiteral("not_started");
};
