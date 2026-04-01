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
#include "shell/ShellWorkflowMenu.h"
#include "shell/ShellStatusBar.h"

#include <QSet>

namespace {

QStringList variantToStringList(const QVariant& value)
{
    if (value.canConvert<QStringList>()) {
        return value.toStringList();
    }

    QStringList result;
    const QVariantList values = value.toList();
    for (const QVariant& item : values) {
        const QString text = item.toString().trimmed();
        if (!text.isEmpty()) {
            result.append(text);
        }
    }
    return result;
}

QString gatewayStateName(ILogicGateway* gateway)
{
    if (!gateway) {
        return QStringLiteral("Disconnected");
    }

    switch (gateway->getConnectionState()) {
    case ILogicGateway::Connected:
        return QStringLiteral("Connected");
    case ILogicGateway::Degraded:
        return QStringLiteral("Degraded");
    case ILogicGateway::Disconnected:
    default:
        return QStringLiteral("Disconnected");
    }
}

}

BaseSoftwareInitializer::BaseSoftwareInitializer(const QString& softwareType, RunMode mode, QObject* parent)
    : QObject(parent)
    , runMode(mode)
    , softwareType(softwareType)
{
}

void BaseSoftwareInitializer::setSoftwareProfile(const QVariantMap& softwareProfile)
{
    m_softwareProfile = softwareProfile;
}

QVariantMap BaseSoftwareInitializer::getSoftwareProfile() const
{
    return m_softwareProfile;
}

void BaseSoftwareInitializer::initialize(MainWindow* mainWindow, LogicRuntime* logicRuntime,
                                         ILogicGateway* gateway, CommunicationHub* commHub)
{
    const QStringList sequence = configuredWorkflowSequence();
    const QString initialModule = configuredInitialModule();

    // 1. Create PageManager and set its stack widget
    m_pageManager = new PageManager(this);
    m_pageManager->setStackWidget(mainWindow->getWorkspaceShell()->getCenterStack());

    // 2. Create GlobalUiManager, set overlay and tool host
    m_globalUiManager = new GlobalUiManager(this);
    m_globalUiManager->setOverlayLayer(mainWindow->getGlobalOverlayLayer());
    m_globalUiManager->setToolHost(mainWindow->getGlobalToolHost());

    // 3. Create ApplicationCoordinator
    m_appCoordinator = new ApplicationCoordinator(
        gateway,
        m_pageManager,
        m_globalUiManager,
        mainWindow->getWorkspaceShell(),
        this);

    // 4. Configure the runtime-owned WorkflowStateMachine
    m_workflowStateMachine = logicRuntime->getWorkflowStateMachine();
    m_workflowStateMachine->setWorkflowSequence(sequence);
    m_workflowStateMachine->setInitialModule(initialModule);
    m_workflowStateMachine->setCurrentModule(QString());

    // 5. Set enterable modules (initially first module is enterable)
    if (!sequence.isEmpty()) {
        QSet<QString> enterable;
        if (getRunMode() == RunMode::Local) {
            for (const QString& moduleId : sequence) {
                enterable.insert(moduleId);
            }
        } else {
            enterable.insert(sequence.first());
        }
        m_workflowStateMachine->setEnterableModules(enterable);
    }

    auto* workflowMenu = new ShellWorkflowMenu(mainWindow->getWorkspaceShell());
    workflowMenu->setWorkflowSequence(sequence);
    workflowMenu->setEnterableModules(m_workflowStateMachine->getEnterableModules());

    auto* statusBar = new ShellStatusBar(mainWindow->getWorkspaceShell());
    statusBar->setWorkflowSequence(sequence);
    statusBar->setEnterableModules(m_workflowStateMachine->getEnterableModules());
    statusBar->setConnectionState(gatewayStateName(gateway));

    if (QWidget* rightShellHost = mainWindow->getWorkspaceShell()->getRightShellHost()) {
        rightShellHost->layout()->addWidget(workflowMenu);
    }
    if (QWidget* bottomShellHost = mainWindow->getWorkspaceShell()->getBottomShellHost()) {
        bottomShellHost->layout()->addWidget(statusBar);
    }
    mainWindow->getWorkspaceShell()->refreshHostVisibility();

    QObject::connect(workflowMenu, &ShellWorkflowMenu::moduleSelected,
                     m_appCoordinator, &ApplicationCoordinator::requestSwitchModule);
    QObject::connect(statusBar, &ShellStatusBar::nextRequested,
                     m_appCoordinator, &ApplicationCoordinator::requestNextStep);
    QObject::connect(statusBar, &ShellStatusBar::prevRequested,
                     m_appCoordinator, &ApplicationCoordinator::requestPrevStep);
    QObject::connect(statusBar, &ShellStatusBar::resyncRequested,
                     m_appCoordinator, &ApplicationCoordinator::requestResync);
    QObject::connect(m_appCoordinator, &ApplicationCoordinator::currentModuleChanged,
                     workflowMenu, &ShellWorkflowMenu::setCurrentModule);
    QObject::connect(m_appCoordinator, &ApplicationCoordinator::currentModuleChanged,
                     statusBar, &ShellStatusBar::setCurrentModule);
    QObject::connect(m_appCoordinator, &ApplicationCoordinator::enterableModulesChanged,
                     workflowMenu, &ShellWorkflowMenu::setEnterableModules);
    QObject::connect(m_appCoordinator, &ApplicationCoordinator::enterableModulesChanged,
                     statusBar, &ShellStatusBar::setEnterableModules);
    QObject::connect(m_appCoordinator, &ApplicationCoordinator::connectionStateChanged,
                     statusBar, &ShellStatusBar::setConnectionState);
    QObject::connect(m_appCoordinator, &ApplicationCoordinator::healthSnapshotChanged,
                     statusBar, &ShellStatusBar::setHealthSnapshot);
    QObject::connect(m_appCoordinator, &ApplicationCoordinator::workflowDecisionChanged,
                     statusBar, &ShellStatusBar::setWorkflowDecision);

    // 6. Register module logic handlers
    registerModuleLogicHandlers(logicRuntime);

    // 7. Register module UIs
    registerModuleUIs(mainWindow, logicRuntime, m_appCoordinator, gateway);

    // 8. Configure additional settings
    configureAdditionalSettings(logicRuntime);

    // 8.1 Register communication sources once module selection has been resolved
    registerCommunicationSources(commHub);

    // 9. Connect gateway notifications back into the UI coordination layer
    QObject::connect(gateway, &ILogicGateway::notificationReceived,
                     m_appCoordinator, &ApplicationCoordinator::onShellNotification);

    if (commHub && getRunMode() == RunMode::Redis) {
        QObject::connect(commHub, &CommunicationHub::controlMessageReceived,
                         logicRuntime, &LogicRuntime::onControlMessageReceived);
        QObject::connect(commHub, &CommunicationHub::serverCommandReceived,
                         logicRuntime, &LogicRuntime::onServerCommandReceived);
        QObject::connect(commHub, &CommunicationHub::stateSampleReceived,
                         logicRuntime, &LogicRuntime::onStateSampleReceived);
        QObject::connect(commHub, &CommunicationHub::communicationError,
                         logicRuntime, &LogicRuntime::onCommunicationError);
        QObject::connect(commHub, &CommunicationHub::communicationIssue,
                 logicRuntime, &LogicRuntime::onCommunicationIssue);
        QObject::connect(commHub, &CommunicationHub::healthSnapshotChanged,
                 logicRuntime, &LogicRuntime::onCommunicationHealthChanged);
        QObject::connect(commHub, &CommunicationHub::connectionStateChanged,
                         logicRuntime, &LogicRuntime::onConnectionStateChanged);

        logicRuntime->onConnectionStateChanged(commHub->getConnectionStateName());
    }

    // 10. Enter the initial module through the standard action path
    UiAction initialAction = UiAction::create(
        UiAction::RequestSwitchModule,
        QStringLiteral("shell"),
        {{QStringLiteral("targetModule"), initialModule}});
    gateway->sendAction(initialAction);
}

void BaseSoftwareInitializer::registerCommunicationSources(CommunicationHub* commHub)
{
    Q_UNUSED(commHub);
}

void BaseSoftwareInitializer::configureAdditionalSettings(LogicRuntime* runtime)
{
    Q_UNUSED(runtime);
}

QStringList BaseSoftwareInitializer::configuredEnabledModules() const
{
    const QStringList defaults = getEnabledModules();
    const QStringList requested = variantToStringList(
        m_softwareProfile.value(QStringLiteral("enabledModules")));
    if (requested.isEmpty()) {
        return defaults;
    }

    QStringList result;
    for (const QString& moduleId : requested) {
        if (defaults.contains(moduleId) && !result.contains(moduleId)) {
            result.append(moduleId);
        }
    }

    return result.isEmpty() ? defaults : result;
}

QStringList BaseSoftwareInitializer::configuredWorkflowSequence() const
{
    const QStringList enabled = configuredEnabledModules();
    const QStringList defaults = getWorkflowSequence();
    const QStringList requested = variantToStringList(
        m_softwareProfile.value(QStringLiteral("workflowSequence")));

    QStringList result;
    const QStringList source = requested.isEmpty() ? defaults : requested;
    for (const QString& moduleId : source) {
        if (enabled.contains(moduleId) && !result.contains(moduleId)) {
            result.append(moduleId);
        }
    }

    for (const QString& moduleId : enabled) {
        if (!result.contains(moduleId)) {
            result.append(moduleId);
        }
    }

    return result;
}

QString BaseSoftwareInitializer::configuredInitialModule() const
{
    const QStringList sequence = configuredWorkflowSequence();
    const QString requested = m_softwareProfile.value(QStringLiteral("initialModule")).toString();
    if (!requested.isEmpty() && sequence.contains(requested)) {
        return requested;
    }

    const QString fallback = getInitialModule();
    if (!fallback.isEmpty() && sequence.contains(fallback)) {
        return fallback;
    }

    return sequence.isEmpty() ? QString() : sequence.first();
}

bool BaseSoftwareInitializer::isModuleEnabled(const QString& moduleId) const
{
    return configuredEnabledModules().contains(moduleId);
}

QString BaseSoftwareInitializer::getSoftwareType() const
{
    return softwareType;
}

RunMode BaseSoftwareInitializer::getRunMode() const
{
    return runMode;
}
