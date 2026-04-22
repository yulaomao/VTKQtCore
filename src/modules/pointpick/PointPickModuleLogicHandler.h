#pragma once

#include "logic/registry/ModuleLogicHandler.h"
#include <QString>

class PointNode;
class SceneGraph;

class PointPickModuleLogicHandler : public ModuleLogicHandler
{
    Q_OBJECT

public:
    explicit PointPickModuleLogicHandler(QObject* parent = nullptr);

    void handleAction(const UiAction& action) override;
    void handlePollData(const QString& key, const QVariant& value) override;
    void handleSubscribeData(const QString& channel, const QVariantMap& data) override;
    void onModuleActivated() override;
    void onModuleDeactivated() override;
    void onResync() override;

private:
    void handleIncomingData(const QVariantMap& payloadData);
    PointNode* ensureSelectionNode(SceneGraph* scene);
    PointNode* findSelectionNode(SceneGraph* scene) const;
    void emitSelectionState(const QString& sourceActionId = QString());

    QString m_pointNodeId;
    bool m_confirmed = false;
};
