#pragma once

#include "logic/registry/ModuleLogicHandler.h"
#include <QString>

class NavigationModuleLogicHandler : public ModuleLogicHandler
{
    Q_OBJECT

public:
    explicit NavigationModuleLogicHandler(QObject* parent = nullptr);

    void handleAction(const UiAction& action) override;
    void onModuleActivated() override;
    void onModuleDeactivated() override;
    void onResync() override;

private:
    QString m_toolTransformNodeId;
    bool m_navigating = false;
};
