#pragma once

#include "BaseSoftwareInitializer.h"
#include "communication/config/RedisDispatchConfig.h"

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

private:
    // Returns the dispatch config, loading it lazily on first access.
    const RedisDispatchConfig& dispatchConfig() const;

    mutable RedisDispatchConfig m_dispatchConfig;
    mutable bool m_dispatchConfigLoaded = false;
};
