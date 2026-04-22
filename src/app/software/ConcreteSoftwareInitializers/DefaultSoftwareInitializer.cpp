#include "DefaultSoftwareInitializer.h"

#include "ModuleUiAssemblers.h"
#include "SoftwareInitializerFactory.h"
#include "ApplicationCoordinator.h"
#include "ILogicGateway.h"
#include "LogicRuntime.h"
#include "MainWindow.h"
#include "communication/MessageDispatchCenter.h"
#include "communication/RedisConnectionConfig.h"
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
#include <QVector>

namespace {

// Build the two Redis connections used by the default software profile.
//
//  conn_main  – DB 0, carries params / pointpick / planning state.
//  conn_nav   – DB 1, carries navigation state and transform frames.
QVector<RedisConnectionConfig> buildDefaultConnections()
{
    RedisConnectionConfig connMain;
    connMain.connectionId   = QStringLiteral("conn_main");
    connMain.host           = QStringLiteral("127.0.0.1");
    connMain.port           = 6379;
    connMain.db             = 0;
    connMain.pollIntervalMs = 16;
    connMain.pollingKeyGroups = {
        { QStringLiteral("params"),    { QStringLiteral("state.params.latest") } },
        { QStringLiteral("pointpick"), { QStringLiteral("state.pointpick.latest") } },
        { QStringLiteral("planning"),  { QStringLiteral("state.planning.latest") } },
    };
    connMain.subscriptionChannels = {
        { QStringLiteral("state.params"),    QStringLiteral("params") },
        { QStringLiteral("state.pointpick"), QStringLiteral("pointpick") },
        { QStringLiteral("state.planning"),  QStringLiteral("planning") },
    };

    RedisConnectionConfig connNav;
    connNav.connectionId   = QStringLiteral("conn_nav");
    connNav.host           = QStringLiteral("127.0.0.1");
    connNav.port           = 6379;
    connNav.db             = 1;
    connNav.pollIntervalMs = 16;
    connNav.pollingKeyGroups = {
        { QStringLiteral("navigation"), { QStringLiteral("state.navigation.latest") } },
        { QStringLiteral("navigation"), {
              QStringLiteral("demo:navigation:transform:world"),
              QStringLiteral("demo:navigation:transform:reference"),
              QStringLiteral("demo:navigation:transform:patient"),
              QStringLiteral("demo:navigation:transform:instrument"),
              QStringLiteral("demo:navigation:transform:guide"),
              QStringLiteral("demo:navigation:transform:tip"),
          }},
    };
    connNav.subscriptionChannels = {
        { QStringLiteral("state.navigation"), QStringLiteral("navigation") },
    };

    return { connMain, connNav };
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

void DefaultSoftwareInitializer::configureDispatchCenter(MessageDispatchCenter* center)
{
    if (!center) {
        return;
    }

    center->configure(buildDefaultConnections());
}

