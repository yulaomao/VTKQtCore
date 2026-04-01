#pragma once

#include "logic/registry/ModuleLogicHandler.h"
#include <QString>

class PointPickModuleLogicHandler : public ModuleLogicHandler
{
    Q_OBJECT

public:
    explicit PointPickModuleLogicHandler(QObject* parent = nullptr);

    void handleAction(const UiAction& action) override;
    void onModuleActivated() override;
    void onModuleDeactivated() override;
    void onResync() override;

private:
    QString m_pointNodeId;
    bool m_confirmed = false;
};
