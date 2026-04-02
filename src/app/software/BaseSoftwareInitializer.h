#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantMap>

class MainWindow;
class LogicRuntime;
class ILogicGateway;
class CommunicationHub;
class ApplicationCoordinator;
class PageManager;
class GlobalUiManager;
class ActiveModuleState;

enum class RunMode { Local, Redis };

class BaseSoftwareInitializer : public QObject
{
    Q_OBJECT

public:
    explicit BaseSoftwareInitializer(const QString& softwareType, RunMode mode, QObject* parent = nullptr);

    void initialize(MainWindow* mainWindow, LogicRuntime* logicRuntime,
                    ILogicGateway* gateway, CommunicationHub* commHub);
    void setSoftwareProfile(const QVariantMap& softwareProfile);
    QVariantMap getSoftwareProfile() const;

    virtual QStringList getEnabledModules() const = 0;
    virtual QStringList getModuleDisplayOrder() const = 0;
    virtual QString getInitialModule() const = 0;

    virtual void registerModuleLogicHandlers(LogicRuntime* runtime) = 0;
    virtual void registerModuleUIs(MainWindow* mainWindow, LogicRuntime* runtime,
                                   ApplicationCoordinator* appCoord,
                                   ILogicGateway* gateway) = 0;
    virtual void registerShellModules(MainWindow* mainWindow, LogicRuntime* runtime,
                                      ApplicationCoordinator* appCoord,
                                      ILogicGateway* gateway);
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
    PageManager* m_pageManager = nullptr;
    GlobalUiManager* m_globalUiManager = nullptr;
    ActiveModuleState* m_activeModuleState = nullptr;

private:
    RunMode runMode;
    QString softwareType;
    QVariantMap m_softwareProfile;
};
