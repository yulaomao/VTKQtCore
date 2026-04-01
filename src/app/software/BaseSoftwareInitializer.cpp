#include "BaseSoftwareInitializer.h"
#include "MainWindow.h"
#include "LogicRuntime.h"
#include "ILogicGateway.h"
#include "CommunicationHub.h"
#include "ApplicationCoordinator.h"
#include "PageManager.h"
#include "GlobalUiManager.h"
#include "WorkflowStateMachine.h"
#include "WorkspaceShell.h"

BaseSoftwareInitializer::BaseSoftwareInitializer(const QString& softwareType, RunMode mode, QObject* parent)
    : QObject(parent)
    , runMode(mode)
    , softwareType(softwareType)
{
}

void BaseSoftwareInitializer::initialize(MainWindow* mainWindow, LogicRuntime* logicRuntime,
                                         ILogicGateway* gateway, CommunicationHub* commHub)
{
    Q_UNUSED(commHub);

    // 1. Create PageManager and set its stack widget
    m_pageManager = new PageManager(this);
    m_pageManager->setStackWidget(mainWindow->getWorkspaceShell()->getCenterStack());

    // 2. Create GlobalUiManager, set overlay and tool host
    m_globalUiManager = new GlobalUiManager(this);
    m_globalUiManager->setOverlayLayer(mainWindow->getGlobalOverlayLayer());
    m_globalUiManager->setToolHost(mainWindow->getGlobalToolHost());

    // 3. Create ApplicationCoordinator
    m_appCoordinator = new ApplicationCoordinator(gateway, m_pageManager, m_globalUiManager, this);

    // 4. Configure WorkflowStateMachine
    m_workflowStateMachine = new WorkflowStateMachine(this);
    m_workflowStateMachine->setWorkflowSequence(getWorkflowSequence());
    m_workflowStateMachine->setInitialModule(getInitialModule());

    // 5. Set enterable modules (initially first module is enterable)
    QStringList sequence = getWorkflowSequence();
    if (!sequence.isEmpty()) {
        QSet<QString> enterable;
        enterable.insert(sequence.first());
        m_workflowStateMachine->setEnterableModules(enterable);
    }

    // 6. Register module logic handlers
    registerModuleLogicHandlers(logicRuntime);

    // 7. Register module UIs
    registerModuleUIs(mainWindow, m_appCoordinator, gateway);

    // 8. Configure additional settings
    configureAdditionalSettings(logicRuntime);

    // 9. Connect LogicRuntime's logicNotification to ApplicationCoordinator::onShellNotification
    QObject::connect(logicRuntime, &LogicRuntime::logicNotification,
                     m_appCoordinator, &ApplicationCoordinator::onShellNotification,
                     Qt::QueuedConnection);

    // 10. Switch to initial module
    m_pageManager->switchToPage(getInitialModule());
}

void BaseSoftwareInitializer::configureAdditionalSettings(LogicRuntime* runtime)
{
    Q_UNUSED(runtime);
}

QString BaseSoftwareInitializer::getSoftwareType() const
{
    return softwareType;
}

RunMode BaseSoftwareInitializer::getRunMode() const
{
    return runMode;
}
