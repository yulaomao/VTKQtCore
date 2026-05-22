#include "DefaultSoftwareInitializer.h"

#include "ModuleUiAssemblers.h"
#include "SoftwareInitializerFactory.h"
#include "ApplicationCoordinator.h"
#include "LogicRuntime.h"
#include "MainWindow.h"
#include "communication/hub/CommunicationHub.h"
#include "logic/runtime/ILogicRuntimePort.h"
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
#include "ReconstructionModuleLogicHandler.h"

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

QString initialConnectionStateName(RunMode mode)
{
    switch (mode) {
    case RunMode::Local:
        return QStringLiteral("Connected");
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
                                                   ILogicRuntimePort* runtimePort)
{
    const ModuleUiAssemblyContext context{
        mainWindow,
        runtime,
        appCoord,
        runtimePort,
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
                                                      ILogicRuntimePort* runtimePort)
{
    if (!mainWindow || !runtime || !appCoord || !runtimePort) {
        return;
    }

    WorkspaceShell* workspaceShell = mainWindow->getWorkspaceShell();
    if (!workspaceShell) {
        return;
    }

    workspaceShell->getRightWidget()->setFixedWidth(320);

    const QStringList workflowSequence = configuredModuleDisplayOrder();
    const QString initialConnectionState = initialConnectionStateName(getRunMode());

    auto* workflowMenu = new ModuleNavigationModule(workspaceShell);
    workflowMenu->setModuleDisplayOrder(workflowSequence);
    workflowMenu->setConnectionState(initialConnectionState);
    workflowMenu->setActionDispatcher(appCoord->getActionDispatcher());

    auto* statusBar = new ModuleStatusBarModule(workspaceShell);
    statusBar->setModuleDisplayOrder(workflowSequence);
    statusBar->setConnectionState(initialConnectionState);
    statusBar->setActionDispatcher(appCoord->getActionDispatcher());

    auto* interModuleSenderDispatcher = new UiActionDispatcher(
        InterModuleTest::senderModuleId(),
        runtimePort,
        workspaceShell);

    auto* topSenderWidget = new InterModuleSenderWidget(
        interModuleSenderDispatcher,
        workspaceShell->getTopWidget());
    auto* topReceiverWidget = new InterModuleReceiverWidget(runtime, workspaceShell->getTopWidget());

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
    QObject::connect(runtime, &LogicRuntime::logicNotification,
                     workflowMenu, &ModuleNavigationModule::onLogicNotification);
}

void DefaultSoftwareInitializer::configureAdditionalSettings(LogicRuntime* runtime)
{
    Q_UNUSED(runtime);
}

void DefaultSoftwareInitializer::registerCommunicationSources(CommunicationHub* commHub)
{
    if (!commHub) {
        return;
    }

    // Configure outbound control channels (unchanged — used for external control
    // messages, ACK, and resync; unrelated to the data polling/subscription path).
    const QVariantMap profile = getSoftwareProfile();
    commHub->setOutboundChannels(
        outboundControlChannelFromProfile(profile),
        ackChannelFromProfile(profile));

    for (const QString& routingChannel : routingChannelsFromProfile(profile)) {
        commHub->addRoutingChannel(routingChannel);
    }

    // Socket mode receives all inbound messages through CommunicationHub and
    // routes them after receipt.
}
