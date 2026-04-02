#pragma once

#include "logic/registry/ModuleLogicHandler.h"
#include <QString>

class SceneGraph;
class TransformNode;

class NavigationModuleLogicHandler : public ModuleLogicHandler
{
    Q_OBJECT

public:
    explicit NavigationModuleLogicHandler(QObject* parent = nullptr);

    void handleAction(const UiAction& action) override;
    void handleStateSample(const StateSample& sample) override;
    void onModuleActivated() override;
    void onModuleDeactivated() override;
    void onResync() override;

private:
    TransformNode* ensureToolTransformNode(SceneGraph* scene);
    TransformNode* findToolTransformNode(SceneGraph* scene) const;
    void emitNavigationState(const QString& sourceActionId = QString(),
                             const QString& sourceSampleId = QString());

    QString m_toolTransformNodeId;
    bool m_navigating = false;
    QString m_navigationStatus = QStringLiteral("Idle");
};
