#pragma once

#include "BaseSoftwareInitializer.h"

class DefaultSoftwareInitializer : public BaseSoftwareInitializer
{
    Q_OBJECT

public:
    explicit DefaultSoftwareInitializer(const QString& softwareType, RunMode mode, QObject* parent = nullptr);

    QStringList getEnabledModules() const override;
    QStringList getModuleDisplayOrder() const override;
    QString getInitialModule() const override;

    void registerModuleLogicHandlers(LogicRuntime* runtime) override;
    void registerModuleUIs(MainWindow* mainWindow, LogicRuntime* runtime,
                           ApplicationCoordinator* appCoord,
                           ILogicGateway* gateway) override;
    void registerShellModules(MainWindow* mainWindow, LogicRuntime* runtime,
                              ApplicationCoordinator* appCoord,
                              ILogicGateway* gateway) override;
    void configureDispatchCenter(MessageDispatchCenter* center) override;
};
