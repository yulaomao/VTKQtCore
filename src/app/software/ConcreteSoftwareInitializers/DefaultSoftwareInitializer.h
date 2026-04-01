#pragma once

#include "BaseSoftwareInitializer.h"

class DefaultSoftwareInitializer : public BaseSoftwareInitializer
{
    Q_OBJECT

public:
    explicit DefaultSoftwareInitializer(const QString& softwareType, RunMode mode, QObject* parent = nullptr);

    QStringList getEnabledModules() const override;
    QStringList getWorkflowSequence() const override;
    QString getInitialModule() const override;

    void registerModuleLogicHandlers(LogicRuntime* runtime) override;
    void registerModuleUIs(MainWindow* mainWindow, ApplicationCoordinator* appCoord,
                           ILogicGateway* gateway) override;
};
