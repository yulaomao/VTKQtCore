#pragma once

#include <QVariantMap>

#include "app/bootstrap/ProductDefinition.h"
#include "BaseSoftwareInitializer.h"

class ProductSoftwareInitializer : public BaseSoftwareInitializer
{
    Q_OBJECT

public:
    explicit ProductSoftwareInitializer(const ProductDefinition& productDefinition,
                                        const QVariantMap& defaultProfile,
                                        RunMode mode,
                                        QObject* parent = nullptr);

    QStringList getEnabledModules() const override;
    QStringList getModuleDisplayOrder() const override;
    QString getInitialModule() const override;

    void registerModuleLogicHandlers(LogicRuntime* runtime) override;
    void registerModuleUIs(MainWindow* mainWindow, LogicRuntime* runtime,
                           ApplicationCoordinator* appCoord,
                           ILogicRuntimePort* runtimePort) override;
    void registerGlobalWidgetFactories(MainWindow* mainWindow, LogicRuntime* runtime,
                                       ApplicationCoordinator* appCoord,
                                       ILogicRuntimePort* runtimePort,
                                       GlobalWidgetRegistry* globalWidgetRegistry) override;
    QWidget* buildProductUi(MainWindow* mainWindow, LogicRuntime* runtime,
                            ApplicationCoordinator* appCoord,
                            ILogicRuntimePort* runtimePort) override;
    void configureAdditionalSettings(LogicRuntime* runtime) override;
    void registerCommunicationSources(CommunicationHub* commHub) override;

private:
    ProductDefinition m_productDefinition;
    QVariantMap m_defaultProfile;
};