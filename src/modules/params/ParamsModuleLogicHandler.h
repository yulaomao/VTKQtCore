#pragma once

#include "logic/registry/ModuleLogicHandler.h"
#include <QVariantMap>

class ParamsModuleLogicHandler : public ModuleLogicHandler
{
    Q_OBJECT

public:
    explicit ParamsModuleLogicHandler(QObject* parent = nullptr);

    void handleAction(const UiAction& action) override;
    void handlePollData(const QString& key, const QVariant& value) override;
    void handleSubscribeData(const QString& channel, const QVariantMap& data) override;
    void onModuleActivated() override;
    void onModuleDeactivated() override;
    void onResync() override;

    QVariantMap getParameters() const;

private:
    QVariantMap m_parameters;
};
