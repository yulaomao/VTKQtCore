#include "BaseSoftwareInitializer.h"
#include "MainWindow.h"
#include "LogicRuntime.h"
#include "ILogicGateway.h"
#include "communication/MessageDispatchCenter.h"
#include "ApplicationCoordinator.h"
#include "PageManager.h"
#include "GlobalUiManager.h"
#include "ActiveModuleState.h"
#include "WorkspaceShell.h"
#include "ui/coordination/UiActionDispatcher.h"

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
                                         ILogicGateway* gateway,
                                         MessageDispatchCenter* dispatchCenter)
{
    const QStringList moduleDisplayOrder = configuredModuleDisplayOrder();
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

    // 4. Configure the runtime-owned active-module state
    m_activeModuleState = logicRuntime->getActiveModuleState();
    m_activeModuleState->setInitialModule(initialModule);
    m_activeModuleState->setCurrentModule(QString());

    // 5. Register module logic handlers
    registerModuleLogicHandlers(logicRuntime);

    // 6. Register module UIs
    registerModuleUIs(mainWindow, logicRuntime, m_appCoordinator, gateway);

    // 7. Register optional shell modules
    registerShellModules(mainWindow, logicRuntime, m_appCoordinator, gateway);

    // 8. Configure additional settings
    configureAdditionalSettings(logicRuntime);

    // 9. Configure the dispatch center (connections and module routing)
    configureDispatchCenter(dispatchCenter);

    // 10. Connect gateway notifications back into the UI coordination layer
    QObject::connect(gateway, &ILogicGateway::notificationReceived,
                     m_appCoordinator, &ApplicationCoordinator::onShellNotification);

    // 11. Enter the initial module through the standard action path
    if (m_appCoordinator && m_appCoordinator->getActionDispatcher()) {
        m_appCoordinator->getActionDispatcher()->requestModuleSwitch(initialModule);
    }
}

void BaseSoftwareInitializer::registerShellModules(MainWindow* mainWindow,
                                                   LogicRuntime* runtime,
                                                   ApplicationCoordinator* appCoord,
                                                   ILogicGateway* gateway)
{
    Q_UNUSED(mainWindow);
    Q_UNUSED(runtime);
    Q_UNUSED(appCoord);
    Q_UNUSED(gateway);
}

void BaseSoftwareInitializer::configureDispatchCenter(MessageDispatchCenter* center)
{
    Q_UNUSED(center);
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

QStringList BaseSoftwareInitializer::configuredModuleDisplayOrder() const
{
    const QStringList enabled = configuredEnabledModules();
    const QStringList defaults = getModuleDisplayOrder();
    QStringList requested = variantToStringList(
        m_softwareProfile.value(QStringLiteral("moduleDisplayOrder")));
    if (requested.isEmpty()) {
        requested = variantToStringList(
            m_softwareProfile.value(QStringLiteral("workflowSequence")));
    }

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
    const QStringList sequence = configuredModuleDisplayOrder();
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
