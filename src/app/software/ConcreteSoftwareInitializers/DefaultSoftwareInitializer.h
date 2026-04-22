#pragma once

#include "BaseSoftwareInitializer.h"
#include "communication/datasource/GlobalPollingPlan.h"

#include <QMap>

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
    void configureAdditionalSettings(LogicRuntime* runtime) override;
    void registerCommunicationSources(CommunicationHub* commHub) override;

protected:
    // Returns the full key-route table that drives the parser and polling plan.
    // Subclasses can override to extend or replace the default routes.
    virtual QMap<QString, PollingKeyRoute> buildKeyRoutes() const;
};
