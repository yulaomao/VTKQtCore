#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class MainWindow;
class LogicRuntime;
class ILogicRuntimePort;
class CommunicationHub;
class ApplicationCoordinator;
class GlobalUiManager;
class GlobalWidgetRegistry;
class ActiveModuleState;
class QWidget;

enum class RunMode { Local, Socket };

class BaseSoftwareInitializer : public QObject
{
    Q_OBJECT

public:
    explicit BaseSoftwareInitializer(const QString& softwareType, RunMode mode, QObject* parent = nullptr);

    void initialize(MainWindow* mainWindow, LogicRuntime* logicRuntime,
                    ILogicRuntimePort* runtimePort, CommunicationHub* commHub);
    void setSoftwareProfile(const QVariantMap& softwareProfile);
    QVariantMap getSoftwareProfile() const;

    virtual QStringList getEnabledModules() const = 0;
    virtual QStringList getModuleDisplayOrder() const = 0;
    virtual QString getInitialModule() const = 0;

    virtual void registerModuleLogicHandlers(LogicRuntime* runtime) = 0;
    virtual void registerModuleUIs(MainWindow* mainWindow, LogicRuntime* runtime,
                                   ApplicationCoordinator* appCoord,
                                              ILogicRuntimePort* runtimePort) = 0;
    virtual void registerGlobalWidgetFactories(MainWindow* mainWindow, LogicRuntime* runtime,
                                               ApplicationCoordinator* appCoord,
                                               ILogicRuntimePort* runtimePort,
                                               GlobalWidgetRegistry* globalWidgetRegistry);
    virtual QWidget* buildProductUi(MainWindow* mainWindow, LogicRuntime* runtime,
                                    ApplicationCoordinator* appCoord,
                                    ILogicRuntimePort* runtimePort) = 0;
    virtual void registerCommunicationSources(CommunicationHub* commHub);
    virtual void configureAdditionalSettings(LogicRuntime* runtime);

    QString getSoftwareType() const;
    RunMode getRunMode() const;

protected:
    QStringList configuredEnabledModules() const;
    QStringList configuredModuleDisplayOrder() const;
    QString configuredInitialModule() const;
    bool isModuleEnabled(const QString& moduleId) const;

    ApplicationCoordinator* m_appCoordinator = nullptr;
    GlobalUiManager* m_globalUiManager = nullptr;
    GlobalWidgetRegistry* m_globalWidgetRegistry = nullptr;
    ActiveModuleState* m_activeModuleState = nullptr;

private:
    RunMode runMode;
    QString softwareType;
    QVariantMap m_softwareProfile;
};
