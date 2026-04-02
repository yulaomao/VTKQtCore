#pragma once

#include "DefaultSoftwareInitializer.h"

class PlanningNavigationSoftwareInitializer : public DefaultSoftwareInitializer
{
    Q_OBJECT

public:
    explicit PlanningNavigationSoftwareInitializer(const QString& softwareType,
                                                   RunMode mode,
                                                   QObject* parent = nullptr);

    QStringList getEnabledModules() const override;
    QStringList getModuleDisplayOrder() const override;
    QString getInitialModule() const override;
};