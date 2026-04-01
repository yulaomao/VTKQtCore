#pragma once

#include "logic/registry/ModuleLogicHandler.h"
#include <QVariantMap>

class ParamsModuleLogicHandler : public ModuleLogicHandler
{
    Q_OBJECT

public:
    explicit ParamsModuleLogicHandler(QObject* parent = nullptr);

    void handleAction(const UiAction& action) override;
    void handleStateSample(const StateSample& sample) override;
    void onModuleActivated() override;
    void onModuleDeactivated() override;
    void onResync() override;

    QVariantMap getParameters() const;

private:
    QVariantMap m_parameters;
};
