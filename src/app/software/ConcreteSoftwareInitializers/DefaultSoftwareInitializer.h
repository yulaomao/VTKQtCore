#pragma once

#include "BaseSoftwareInitializer.h"
#include "communication/redis/RedisConnectionConfig.h"

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
    // Returns the connection configs, loading from JSON lazily on first access.
    const QVector<RedisConnectionConfig>& connectionConfigs() const;

    mutable QVector<RedisConnectionConfig> m_connectionConfigs;
    mutable bool m_configLoaded = false;
};
