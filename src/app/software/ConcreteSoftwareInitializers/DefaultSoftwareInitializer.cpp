#include "DefaultSoftwareInitializer.h"

#include "DefaultGlobalPollingSampleParser.h"
#include "ModuleUiAssemblers.h"
#include "SoftwareInitializerFactory.h"
#include "ApplicationCoordinator.h"
#include "ILogicGateway.h"
#include "LogicRuntime.h"
#include "MainWindow.h"
#include "communication/datasource/GlobalPollingPlan.h"
#include "communication/hub/CommunicationHub.h"
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

#include <QHBoxLayout>
#include <QLayout>
#include <QMap>
#include <QVector>

namespace {

QString stringFromVariantOrDefault(const QVariant& value, const QString& fallback)
{
    const QString text = value.toString().trimmed();
    return text.isEmpty() ? fallback : text;
}

QString defaultSubscriptionChannel(const QString& moduleId)
{
    return QStringLiteral("state.%1").arg(moduleId);
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

QString defaultPollingKey(const QString& moduleId)
{
    return QStringLiteral("state.%1.latest").arg(moduleId);
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

// Build a GlobalPollingPlan from a route table, so the key list is always
// in sync with the routes and no key is accidentally omitted.
GlobalPollingPlan createPollingPlanFromRoutes(const QMap<QString, PollingKeyRoute>& routes)
{
    QStringList keys;
    keys.reserve(routes.size());
    for (auto it = routes.cbegin(); it != routes.cend(); ++it) {
        keys.append(it.key());
    }

    GlobalPollingPlan plan(QStringLiteral("framework_global_poll"), keys, 16);
    plan.setChangeDetection(true);
    plan.setMaxDispatchRateHz(60.0);
    plan.setActive(true);
    plan.setKeyRoutes(routes);
    return plan;
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

    auto* parser = new DefaultGlobalPollingSampleParser(runtime);
    parser->setKeyRoutes(buildKeyRoutes());
    runtime->setGlobalPollingSampleParser(parser);
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

    commHub->setGlobalPollingPlan(createPollingPlanFromRoutes(buildKeyRoutes()));
}

QMap<QString, PollingKeyRoute> DefaultSoftwareInitializer::buildKeyRoutes() const
{
    QMap<QString, PollingKeyRoute> routes;

    // ── params ───────────────────────────────────────────────────────────────
    routes.insert(defaultPollingKey(QStringLiteral("params")),
                  {QStringLiteral("params"), QStringLiteral("state")});

    // ── pointpick ─────────────────────────────────────────────────────────────
    routes.insert(defaultPollingKey(QStringLiteral("pointpick")),
                  {QStringLiteral("pointpick"), QStringLiteral("state")});

    // ── planning ──────────────────────────────────────────────────────────────
    routes.insert(defaultPollingKey(QStringLiteral("planning")),
                  {QStringLiteral("planning"), QStringLiteral("state")});

    // ── navigation ────────────────────────────────────────────────────────────
    routes.insert(defaultPollingKey(QStringLiteral("navigation")),
                  {QStringLiteral("navigation"), QStringLiteral("state")});

    routes.insert(QStringLiteral("demo:navigation:transform:world"),
                  {QStringLiteral("navigation"), QStringLiteral("transform:world")});
    routes.insert(QStringLiteral("demo:navigation:transform:reference"),
                  {QStringLiteral("navigation"), QStringLiteral("transform:reference")});
    routes.insert(QStringLiteral("demo:navigation:transform:patient"),
                  {QStringLiteral("navigation"), QStringLiteral("transform:patient")});
    routes.insert(QStringLiteral("demo:navigation:transform:instrument"),
                  {QStringLiteral("navigation"), QStringLiteral("transform:instrument")});
    routes.insert(QStringLiteral("demo:navigation:transform:guide"),
                  {QStringLiteral("navigation"), QStringLiteral("transform:guide")});
    routes.insert(QStringLiteral("demo:navigation:transform:tip"),
                  {QStringLiteral("navigation"), QStringLiteral("transform:tip")});

    return routes;
}
