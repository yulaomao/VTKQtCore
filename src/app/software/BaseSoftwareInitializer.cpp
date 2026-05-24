#include "BaseSoftwareInitializer.h"
#include "MainWindow.h"
#include "LogicRuntime.h"
#include "logic/runtime/ILogicRuntimePort.h"
#include "CommunicationHub.h"
#include "ApplicationCoordinator.h"
#include "GlobalUiManager.h"
#include "ui/globalui/GlobalWidgetRegistry.h"
#include "ActiveModuleState.h"
#include "app/audio/PromptAudioService.h"
#include "contracts/PromptAudioPresetIds.h"
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

void connectDispatcherToCommunicationHub(UiActionDispatcher* dispatcher,
                                         CommunicationHub* commHub)
{
    if (!dispatcher || !commHub) {
        return;
    }

    QObject::connect(dispatcher, &UiActionDispatcher::actionDispatched,
                     commHub, [commHub](const UiAction& action) {
                         commHub->sendActionRequest(action, false);
                     });
    QObject::connect(dispatcher, &UiActionDispatcher::resyncRequested,
                     commHub, [commHub](const QString& reason) {
                         commHub->sendResyncRequest(reason, false);
                     });
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
                                         ILogicRuntimePort* runtimePort, CommunicationHub* commHub)
{
    const QString initialModule = configuredInitialModule();

    // 1. Create global UI services.
    m_globalUiManager = new GlobalUiManager(this);
    m_globalUiManager->setOverlayLayer(mainWindow->getGlobalOverlayLayer());
    m_globalUiManager->setToolHost(mainWindow->getGlobalToolHost());
    m_globalWidgetRegistry = new GlobalWidgetRegistry(this);
    mainWindow->setGlobalWidgetRegistry(m_globalWidgetRegistry);

    // 2. Create application coordination without a built-in shell host.
    m_appCoordinator = new ApplicationCoordinator(
        runtimePort,
        m_globalUiManager,
        this);

    // 3. Configure the runtime-owned active-module state.
    m_activeModuleState = logicRuntime->getActiveModuleState();
    m_activeModuleState->setInitialModule(initialModule);
    m_activeModuleState->setCurrentModule(QString());

    // 3.1 Create the application-wide prompt audio service before any module logic is registered.
    auto* promptAudioService = new PromptAudioService(this);
    logicRuntime->setPromptAudioService(promptAudioService);
    logicRuntime->registerPromptAudioPreset(
        PromptAudioPresetIds::pollingProgress(),
        QStringLiteral(":/audio/prompts/news.wav"));
    logicRuntime->registerPromptAudioPreset(
        PromptAudioPresetIds::pollingAttention(),
        QStringLiteral(":/audio/prompts/news_anchor_female.wav"));

    // 4. Register module logic handlers.
    registerModuleLogicHandlers(logicRuntime);

    // 5. Register module UIs and global widget factories.
    registerModuleUIs(mainWindow, logicRuntime, m_appCoordinator, runtimePort);
    registerGlobalWidgetFactories(mainWindow, logicRuntime, m_appCoordinator,
                                  runtimePort, m_globalWidgetRegistry);

    // 6. Let the concrete initializer provide the product root widget tree.
    if (QWidget* productRoot = buildProductUi(mainWindow, logicRuntime,
                                              m_appCoordinator, runtimePort)) {
        mainWindow->setWorkspaceRootWidget(productRoot);
    }

    // 7. Configure additional settings.
    configureAdditionalSettings(logicRuntime);

    if (commHub && getRunMode() == RunMode::Socket) {
        for (UiActionDispatcher* dispatcher : findChildren<UiActionDispatcher*>()) {
            connectDispatcherToCommunicationHub(dispatcher, commHub);
        }
        for (UiActionDispatcher* dispatcher : mainWindow->findChildren<UiActionDispatcher*>()) {
            connectDispatcherToCommunicationHub(dispatcher, commHub);
        }
    }

    // 8. Register communication sources once module selection has been resolved.
    registerCommunicationSources(commHub);

    // 9. Connect runtime notifications back into the UI coordination layer.
    QObject::connect(logicRuntime, &LogicRuntime::logicNotification,
                     m_appCoordinator, &ApplicationCoordinator::onShellNotification);

    if (commHub && getRunMode() == RunMode::Socket) {
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

    // 10. Enter the initial module through direct runtime activation.
    logicRuntime->initializeActiveModule(initialModule);
}

void BaseSoftwareInitializer::registerGlobalWidgetFactories(MainWindow* mainWindow,
                                                            LogicRuntime* runtime,
                                                            ApplicationCoordinator* appCoord,
                                                            ILogicRuntimePort* runtimePort,
                                                            GlobalWidgetRegistry* globalWidgetRegistry)
{
    Q_UNUSED(mainWindow);
    Q_UNUSED(runtime);
    Q_UNUSED(appCoord);
    Q_UNUSED(runtimePort);
    Q_UNUSED(globalWidgetRegistry);
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
