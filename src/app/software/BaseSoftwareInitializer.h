#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class MainWindow;
class LogicRuntime;
class ILogicGateway;
class CommunicationHub;
class ApplicationCoordinator;
class PageManager;
class GlobalUiManager;

enum class RunMode { Local, Redis };

class BaseSoftwareInitializer : public QObject
{
    Q_OBJECT

public:
    explicit BaseSoftwareInitializer(const QString& softwareType, RunMode mode, QObject* parent = nullptr);

    void initialize(MainWindow* mainWindow, LogicRuntime* logicRuntime,
                    ILogicGateway* gateway, CommunicationHub* commHub);

    virtual QStringList getEnabledModules() const = 0;
    virtual QStringList getWorkflowSequence() const = 0;
    virtual QString getInitialModule() const = 0;

    virtual void registerModuleLogicHandlers(LogicRuntime* runtime) = 0;
    virtual void registerModuleUIs(MainWindow* mainWindow, ApplicationCoordinator* appCoord,
                                   ILogicGateway* gateway) = 0;
    virtual void configureAdditionalSettings(LogicRuntime* runtime);

    QString getSoftwareType() const;
    RunMode getRunMode() const;

protected:
    ApplicationCoordinator* m_appCoordinator = nullptr;
    PageManager* m_pageManager = nullptr;
    GlobalUiManager* m_globalUiManager = nullptr;
    WorkflowStateMachine* m_workflowStateMachine = nullptr;

private:
    RunMode runMode;
    QString softwareType;
};
