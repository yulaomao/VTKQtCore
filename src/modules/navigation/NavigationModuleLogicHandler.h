#pragma once

#include "logic/registry/ModuleLogicHandler.h"

#include <QMap>
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
    TransformNode* ensureTrackedTransformNode(SceneGraph* scene,
                                              const QString& remoteNodeId);
    TransformNode* findTrackedTransformNode(SceneGraph* scene,
                                            const QString& remoteNodeId) const;
    void applyTransformSample(const QVariantMap& payload);
    void emitNavigationState(const QString& sourceActionId = QString(),
                             const QString& sourceSampleId = QString());
    void emitTransformHealth(bool force = false,
                             const QString& sourceSampleId = QString());

    QMap<QString, QString> m_localTransformNodeIdsByRemoteId;
    QMap<QString, qint64> m_lastSampleTimestampMsByRemoteId;
    bool m_navigating = false;
    QString m_navigationStatus = QStringLiteral("Idle");
    double m_currentPositionX = 0.0;
    double m_currentPositionY = 0.0;
    double m_currentPositionZ = 0.0;
    qint64 m_lastTransformHealthEmitMs = 0;
};
