#pragma once

#include "logic/registry/ModuleLogicHandler.h"
#include <QString>

class PlanningModuleLogicHandler : public ModuleLogicHandler
{
    Q_OBJECT

public:
    explicit PlanningModuleLogicHandler(QObject* parent = nullptr);

    void handleAction(const UiAction& action) override;
    void onModuleActivated() override;
    void onModuleDeactivated() override;
    void onResync() override;

private:
    QString m_planModelNodeId;
    QString m_planPathNodeId;
};
