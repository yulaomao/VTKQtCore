#include "DefaultSoftwareInitializer.h"

#include "ConfigDrivenSampleParser.h"
#include "ModuleUiAssemblers.h"
#include "SoftwareInitializerFactory.h"
#include "ApplicationCoordinator.h"
#include "ILogicGateway.h"
#include "LogicRuntime.h"
#include "MainWindow.h"
#include "communication/config/RedisDispatchConfig.h"
#include "communication/config/RedisDispatchConfigLoader.h"
#include "communication/datasource/SubscriptionSource.h"
#include "communication/hub/CommunicationHub.h"
#include "logic/registry/ModuleLogicHandler.h"
#include "logic/registry/ModuleLogicRegistry.h"
#include "modules/intermoduletest/InterModuleReceiverLogicHandler.h"
#include "modules/intermoduletest/InterModuleReceiverWidget.h"
#include "modules/intermoduletest/InterModuleSenderLogicHandler.h"
#include "modules/intermoduletest/InterModuleSenderWidget.h"
#include "modules/intermoduletest/InterModuleTestConstants.h"
#include "modules/workflowshell/ModuleNavigationModule.h"
#include "modules/workflowshell/ModuleStatusBarModule.h"
#include "shell/WorkspaceShell.h"
#include "ui/coordination/UiActionDispatcher.h"

#include "ParamsModuleLogicHandler.h"
#include "DataGenModuleLogicHandler.h"
#include "PointPickModuleLogicHandler.h"
#include "PlanningModuleLogicHandler.h"
#include "NavigationModuleLogicHandler.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QLayout>
#include <QVector>

namespace {

QString stringFromVariantOrDefault(const QVariant& value, const QString& fallback)
{
    const QString text = value.toString().trimmed();
    return text.isEmpty() ? fallback : text;
}

QString defaultControlRoutingChannel()
{
    return QStringLiteral("control.downstream");
}

QString defaultControlPublishChannel()
{
    return QStringLiteral("control.upstream");
}

QString defaultAckChannel()
{
    return QStringLiteral("control.ack");
}

QVariantMap communicationProfile(const QVariantMap& profile)
{
    return profile.value(QStringLiteral("communication")).toMap();
}

QStringList stringListFromVariant(const QVariant& value)
{
    if (value.canConvert<QStringList>()) {
        return value.toStringList();
    }

    QStringList result;
    const QVariantList list = value.toList();
    for (const QVariant& item : list) {
        const QString text = item.toString().trimmed();
        if (!text.isEmpty()) {
            result.append(text);
        }
    }
    return result;
}

QStringList routingChannelsFromProfile(const QVariantMap& profile)
{
    const QStringList channels = stringListFromVariant(
        communicationProfile(profile).value(QStringLiteral("routingChannels")));
    return channels.isEmpty() ? QStringList{defaultControlRoutingChannel()} : channels;
}

QString outboundControlChannelFromProfile(const QVariantMap& profile)
{
    return stringFromVariantOrDefault(
        communicationProfile(profile).value(QStringLiteral("controlPublishChannel")),
        defaultControlPublishChannel());
}

QString ackChannelFromProfile(const QVariantMap& profile)
{
    return stringFromVariantOrDefault(
        communicationProfile(profile).value(QStringLiteral("ackChannel")),
        defaultAckChannel());
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

const bool s_registered = [] {
    SoftwareInitializerFactory::registerInitializer(
        QStringLiteral("default"),
        [](const QString& softwareType, RunMode mode, QObject* parent) -> BaseSoftwareInitializer* {
            return new DefaultSoftwareInitializer(softwareType, mode, parent);
        });
    return true;
}();

}

DefaultSoftwareInitializer::DefaultSoftwareInitializer(const QString& softwareType,
                                                       RunMode mode,
                                                       QObject* parent)
    : BaseSoftwareInitializer(softwareType, mode, parent)
{
}

QStringList DefaultSoftwareInitializer::getEnabledModules() const
{
    return {
        QStringLiteral("datagen"),
        QStringLiteral("params"),
        QStringLiteral("pointpick"),
        QStringLiteral("planning"),
        QStringLiteral("navigation")
    };
}

QStringList DefaultSoftwareInitializer::getModuleDisplayOrder() const
{
    return {
        QStringLiteral("datagen"),
        QStringLiteral("params"),
        QStringLiteral("pointpick"),
        QStringLiteral("planning"),
        QStringLiteral("navigation")
    };
}

QString DefaultSoftwareInitializer::getInitialModule() const
{
    return QStringLiteral("datagen");
}

void DefaultSoftwareInitializer::registerModuleLogicHandlers(LogicRuntime* runtime)
{
    if (!runtime) {
        return;
    }

    runtime->registerModuleHandler(new InterModuleSenderLogicHandler(runtime));
    runtime->registerModuleHandler(new InterModuleReceiverLogicHandler(runtime));

    if (isModuleEnabled(QStringLiteral("datagen"))) {
        runtime->registerModuleHandler(new DataGenModuleLogicHandler(runtime));
    }
    if (isModuleEnabled(QStringLiteral("params"))) {
        runtime->registerModuleHandler(new ParamsModuleLogicHandler(runtime));
    }
    if (isModuleEnabled(QStringLiteral("pointpick"))) {
        runtime->registerModuleHandler(new PointPickModuleLogicHandler(runtime));
    }
    if (isModuleEnabled(QStringLiteral("planning"))) {
        runtime->registerModuleHandler(new PlanningModuleLogicHandler(runtime));
    }
    if (isModuleEnabled(QStringLiteral("navigation"))) {
        runtime->registerModuleHandler(new NavigationModuleLogicHandler(runtime));
    }

    // Inject the default connectionId from the dispatch config into each handler
    // so that modules know which connection to use for direct Redis access.
    const RedisDispatchConfig& config = dispatchConfig();
    if (config.isValid()) {
        ModuleLogicRegistry* registry = runtime->getModuleLogicRegistry();
        if (registry) {
            for (const RedisDispatchConfig::ModuleEntry& entry : config.modules) {
                ModuleLogicHandler* handler = registry->getHandler(entry.moduleId);
                if (handler && !entry.defaultConnectionId.isEmpty()) {
                    handler->setDefaultConnectionId(entry.defaultConnectionId);
                }
            }
        }
    }
}

void DefaultSoftwareInitializer::registerModuleUIs(MainWindow* mainWindow,
                                                   LogicRuntime* runtime,
                                                   ApplicationCoordinator* appCoord,
                                                   ILogicGateway* gateway)
{
    const ModuleUiAssemblyContext context{
        mainWindow,
        runtime,
        appCoord,
        gateway,
        m_pageManager,
        m_globalUiManager
    };

    if (isModuleEnabled(QStringLiteral("datagen"))) {
        registerDataGenModuleUi(context);
    }
    if (isModuleEnabled(QStringLiteral("params"))) {
        registerParamsModuleUi(context);
    }
    if (isModuleEnabled(QStringLiteral("pointpick"))) {
        registerPointPickModuleUi(context);
    }
    if (isModuleEnabled(QStringLiteral("planning"))) {
        registerPlanningModuleUi(context);
    }
    if (isModuleEnabled(QStringLiteral("navigation"))) {
        registerNavigationModuleUi(context);
    }
}

void DefaultSoftwareInitializer::registerShellModules(MainWindow* mainWindow,
                                                      LogicRuntime* runtime,
                                                      ApplicationCoordinator* appCoord,
                                                      ILogicGateway* gateway)
{
    Q_UNUSED(runtime);

    if (!mainWindow || !appCoord || !gateway) {
        return;
    }

    WorkspaceShell* workspaceShell = mainWindow->getWorkspaceShell();
    if (!workspaceShell) {
        return;
    }

    workspaceShell->getRightWidget()->setFixedWidth(320);

    const QStringList workflowSequence = configuredModuleDisplayOrder();

    auto* workflowMenu = new ModuleNavigationModule(workspaceShell);
    workflowMenu->setModuleDisplayOrder(workflowSequence);
    workflowMenu->setConnectionState(gatewayStateName(gateway));
    workflowMenu->setActionDispatcher(appCoord->getActionDispatcher());

    auto* statusBar = new ModuleStatusBarModule(workspaceShell);
    statusBar->setModuleDisplayOrder(workflowSequence);
    statusBar->setConnectionState(gatewayStateName(gateway));
    statusBar->setActionDispatcher(appCoord->getActionDispatcher());

    auto* interModuleSenderDispatcher = new UiActionDispatcher(
        InterModuleTest::senderModuleId(),
        gateway,
        workspaceShell);

    auto* topSenderWidget = new InterModuleSenderWidget(
        interModuleSenderDispatcher,
        workspaceShell->getTopWidget());
    auto* topReceiverWidget = new InterModuleReceiverWidget(gateway, workspaceShell->getTopWidget());

    if (auto* topLayout = qobject_cast<QHBoxLayout*>(workspaceShell->getTopWidget()->layout())) {
        topLayout->addWidget(topSenderWidget, 0, Qt::AlignLeft | Qt::AlignVCenter);
        topLayout->addStretch(1);
        topLayout->addWidget(topReceiverWidget, 0, Qt::AlignRight | Qt::AlignVCenter);
    }

    if (QWidget* rightShellHost = workspaceShell->getRightShellHost()) {
        rightShellHost->layout()->addWidget(workflowMenu);
    }
    if (QWidget* bottomShellHost = workspaceShell->getBottomShellHost()) {
        bottomShellHost->layout()->addWidget(statusBar);
    }
    workspaceShell->refreshHostVisibility();

    QObject::connect(appCoord, &ApplicationCoordinator::currentModuleChanged,
                     workflowMenu, &ModuleNavigationModule::setCurrentModule);
    QObject::connect(appCoord, &ApplicationCoordinator::currentModuleChanged,
                     statusBar, &ModuleStatusBarModule::setCurrentModule);
    QObject::connect(appCoord, &ApplicationCoordinator::connectionStateChanged,
                     workflowMenu, &ModuleNavigationModule::setConnectionState);
    QObject::connect(appCoord, &ApplicationCoordinator::connectionStateChanged,
                     statusBar, &ModuleStatusBarModule::setConnectionState);
    QObject::connect(appCoord, &ApplicationCoordinator::healthSnapshotChanged,
                     statusBar, &ModuleStatusBarModule::setHealthSnapshot);
    QObject::connect(gateway, &ILogicGateway::notificationReceived,
                     workflowMenu, &ModuleNavigationModule::onGatewayNotification);
}

void DefaultSoftwareInitializer::configureAdditionalSettings(LogicRuntime* runtime)
{
    if (!runtime) {
        return;
    }

    const RedisDispatchConfig& config = dispatchConfig();
    if (!config.isValid()) {
        return;
    }

    runtime->setGlobalPollingSampleParser(new ConfigDrivenSampleParser(config, runtime));
}

void DefaultSoftwareInitializer::registerCommunicationSources(CommunicationHub* commHub)
{
    if (!commHub) {
        return;
    }

    const QVariantMap profile = getSoftwareProfile();
    commHub->setOutboundChannels(
        outboundControlChannelFromProfile(profile),
        ackChannelFromProfile(profile));
    for (const QString& routingChannel : routingChannelsFromProfile(profile)) {
        commHub->addRoutingChannel(routingChannel);
    }

    const RedisDispatchConfig& config = dispatchConfig();
    if (!config.isValid()) {
        return;
    }

    for (const RedisDispatchConfig::ConnectionEntry& entry : config.connections) {
        commHub->addPollingConnection(entry);

        for (const RedisDispatchConfig::SubscriptionChannelEntry& sub :
             entry.subscriptionChannels)
        {
            if (sub.channel.isEmpty() || sub.module.isEmpty()) {
                continue;
            }
            const QString sourceId = QStringLiteral("%1_%2").arg(
                entry.connectionId, sub.channel);
            commHub->addSubscriptionSource(
                new SubscriptionSource(sourceId, sub.channel, sub.module));
        }
    }
}

const RedisDispatchConfig& DefaultSoftwareInitializer::dispatchConfig() const
{
    if (!m_dispatchConfigLoaded) {
        m_dispatchConfigLoaded = true;
        m_dispatchConfig =
            RedisDispatchConfigLoader::loadFromFile(
                QStringLiteral(":/redis_dispatch_config.json"));
        if (!m_dispatchConfig.isValid()) {
            qWarning().noquote()
                << QStringLiteral("[DefaultSoftwareInitializer] redis_dispatch_config.json"
                                  " not found or invalid — falling back to hardcoded defaults");
        }
    }
    return m_dispatchConfig;
}

