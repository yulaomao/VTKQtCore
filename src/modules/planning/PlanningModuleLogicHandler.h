#pragma once

#include "logic/registry/ModuleLogicHandler.h"
#include <QString>

class SceneGraph;
class ModelNode;
class LineNode;

class PlanningModuleLogicHandler : public ModuleLogicHandler
{
    Q_OBJECT

public:
    explicit PlanningModuleLogicHandler(QObject* parent = nullptr);

    void handleAction(const UiAction& action) override;
    void handleStateSample(const StateSample& sample) override;
    void onModuleActivated() override;
    void onModuleDeactivated() override;
    void onResync() override;

private:
    ModelNode* ensurePlanModelNode(SceneGraph* scene);
    LineNode* ensurePlanPathNode(SceneGraph* scene);
    ModelNode* findPlanModelNode(SceneGraph* scene) const;
    LineNode* findPlanPathNode(SceneGraph* scene) const;
    void ensureDefaultPlanGeometry(ModelNode* modelNode, LineNode* pathNode) const;
    void emitPlanningState(LogicNotification::EventType eventType,
                           const QString& sourceActionId = QString(),
                           const QString& sourceSampleId = QString()) const;

    QString m_planModelNodeId;
    QString m_planPathNodeId;
    QString m_planStatus = QStringLiteral("not_started");
};
