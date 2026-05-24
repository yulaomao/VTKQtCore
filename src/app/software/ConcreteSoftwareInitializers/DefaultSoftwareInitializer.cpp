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
#include "ui/globalui/GlobalWidgetRegistry.h"
#include "modules/intermoduletest/InterModuleReceiverLogicHandler.h"
#include "modules/intermoduletest/InterModuleReceiverWidget.h"
#include "modules/intermoduletest/InterModuleSenderLogicHandler.h"
#include "modules/intermoduletest/InterModuleSenderWidget.h"
#include "modules/intermoduletest/InterModuleTestConstants.h"
#include "modules/workflowshell/ModuleNavigationModule.h"
#include "modules/workflowshell/ModuleStatusBarModule.h"
#include "ui/coordination/ModuleCoordinator.h"
#include "ui/coordination/UiActionDispatcher.h"

#include "ParamsModuleLogicHandler.h"
#include "DataGenModuleLogicHandler.h"
#include "PointPickModuleLogicHandler.h"
#include "PlanningModuleLogicHandler.h"
#include "NavigationModuleLogicHandler.h"
#include "modules/reconstruction/ReconstructionModuleLogicHandler.h"

#include <QDebug>
#include <QFrame>
#include <QHBoxLayout>
#include <QLayout>
#include <QStackedWidget>
#include <QVector>
#include <QVBoxLayout>

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

bool layoutHasVisibleWidgets(const QLayout* layout)
{
    return layout && layout->count() > 0;
}

void clearLayoutWithoutDeletingWidgets(QLayout* layout)
{
    if (!layout) {
        return;
    }

    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
        }
        delete item;
    }
}

class DefaultProductRoot final : public QWidget
{
public:
    explicit DefaultProductRoot(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        auto* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);

        m_topBar = new QWidget(this);
        m_topBar->setObjectName(QStringLiteral("defaultProductTopBar"));
        m_topBar->setFixedHeight(48);
        m_topLayout = new QHBoxLayout(m_topBar);
        m_topLayout->setContentsMargins(8, 8, 8, 4);
        m_topLayout->setSpacing(8);
        mainLayout->addWidget(m_topBar);

        auto* contentContainer = new QWidget(this);
        auto* contentLayout = new QHBoxLayout(contentContainer);
        contentLayout->setContentsMargins(0, 0, 0, 0);
        contentLayout->setSpacing(0);

        m_pageStack = new QStackedWidget(contentContainer);
        contentLayout->addWidget(m_pageStack, 1);

        m_sidePanel = new QWidget(contentContainer);
        m_sidePanel->setObjectName(QStringLiteral("defaultProductSidePanel"));
        m_sidePanel->setFixedWidth(320);
        auto* sideLayout = new QVBoxLayout(m_sidePanel);
        sideLayout->setContentsMargins(8, 8, 8, 8);
        sideLayout->setSpacing(8);

        m_staticSideHost = new QWidget(m_sidePanel);
        m_staticSideLayout = new QVBoxLayout(m_staticSideHost);
        m_staticSideLayout->setContentsMargins(0, 0, 0, 0);
        m_staticSideLayout->setSpacing(8);

        m_supplementaryHost = new QWidget(m_sidePanel);
        m_supplementaryLayout = new QVBoxLayout(m_supplementaryHost);
        m_supplementaryLayout->setContentsMargins(0, 0, 0, 0);
        m_supplementaryLayout->setSpacing(8);

        sideLayout->addWidget(m_staticSideHost);
        sideLayout->addWidget(m_supplementaryHost);
        sideLayout->addStretch(1);

        contentLayout->addWidget(m_sidePanel);
        mainLayout->addWidget(contentContainer, 1);

        m_bottomBar = new QWidget(this);
        m_bottomBar->setObjectName(QStringLiteral("defaultProductBottomBar"));
        m_bottomBar->setFixedHeight(52);
        m_bottomLayout = new QHBoxLayout(m_bottomBar);
        m_bottomLayout->setContentsMargins(8, 4, 8, 8);
        m_bottomLayout->setSpacing(8);
        mainLayout->addWidget(m_bottomBar);

        refreshVisibility();
    }

    QStackedWidget* pageStack() const
    {
        return m_pageStack;
    }

    QHBoxLayout* topLayout() const
    {
        return m_topLayout;
    }

    QVBoxLayout* staticSideLayout() const
    {
        return m_staticSideLayout;
    }

    QHBoxLayout* bottomLayout() const
    {
        return m_bottomLayout;
    }

    void showSupplementaryViews(const QVector<QWidget*>& widgets)
    {
        clearLayoutWithoutDeletingWidgets(m_supplementaryLayout);
        for (QWidget* widget : widgets) {
            if (!widget) {
                continue;
            }

            widget->setParent(m_supplementaryHost);
            m_supplementaryLayout->addWidget(widget);
            widget->show();
        }
        refreshVisibility();
    }

    void refreshVisibility()
    {
        m_sidePanel->setVisible(layoutHasVisibleWidgets(m_staticSideLayout) ||
                                layoutHasVisibleWidgets(m_supplementaryLayout));
        m_bottomBar->setVisible(layoutHasVisibleWidgets(m_bottomLayout));
    }

private:
    QWidget* m_topBar = nullptr;
    QHBoxLayout* m_topLayout = nullptr;
    QStackedWidget* m_pageStack = nullptr;
    QWidget* m_sidePanel = nullptr;
    QWidget* m_staticSideHost = nullptr;
    QVBoxLayout* m_staticSideLayout = nullptr;
    QWidget* m_supplementaryHost = nullptr;
    QVBoxLayout* m_supplementaryLayout = nullptr;
    QWidget* m_bottomBar = nullptr;
    QHBoxLayout* m_bottomLayout = nullptr;
};

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
    Q_UNUSED(mainWindow);

    const ModuleUiAssemblyContext context{
        runtime,
        appCoord,
        runtimePort,
        m_globalUiManager,
        m_globalWidgetRegistry
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

void DefaultSoftwareInitializer::registerGlobalWidgetFactories(
    MainWindow* mainWindow,
    LogicRuntime* runtime,
    ApplicationCoordinator* appCoord,
    ILogicRuntimePort* runtimePort,
    GlobalWidgetRegistry* globalWidgetRegistry)
{
    Q_UNUSED(mainWindow);
    Q_UNUSED(appCoord);

    if (!runtime || !runtimePort || !globalWidgetRegistry) {
        return;
    }

    globalWidgetRegistry->registerFactory(
        QStringLiteral("intermodule.sender"),
        [runtimePort](QWidget* parent) -> QWidget* {
            auto* dispatcher = new UiActionDispatcher(
                InterModuleTest::senderModuleId(),
                runtimePort,
                parent);
            auto* widget = new InterModuleSenderWidget(dispatcher, parent);
            dispatcher->setParent(widget);
            return widget;
        });
    globalWidgetRegistry->registerFactory(
        QStringLiteral("intermodule.receiver"),
        [runtime](QWidget* parent) -> QWidget* {
            return new InterModuleReceiverWidget(runtime, parent);
        });
}

QWidget* DefaultSoftwareInitializer::buildProductUi(MainWindow* mainWindow,
                                                    LogicRuntime* runtime,
                                                    ApplicationCoordinator* appCoord,
                                                    ILogicRuntimePort* runtimePort)
{
    Q_UNUSED(runtimePort);

    if (!mainWindow || !runtime || !appCoord) {
        return nullptr;
    }

    auto* productRoot = new DefaultProductRoot(mainWindow);
    const QStringList workflowSequence = configuredModuleDisplayOrder();
    const QString initialConnectionState = initialConnectionStateName(getRunMode());

    auto* workflowMenu = new ModuleNavigationModule(productRoot);
    workflowMenu->setModuleDisplayOrder(workflowSequence);
    workflowMenu->setConnectionState(initialConnectionState);
    workflowMenu->setActionDispatcher(appCoord->getActionDispatcher());

    auto* statusBar = new ModuleStatusBarModule(productRoot);
    statusBar->setModuleDisplayOrder(workflowSequence);
    statusBar->setConnectionState(initialConnectionState);
    statusBar->setActionDispatcher(appCoord->getActionDispatcher());

    productRoot->staticSideLayout()->addWidget(workflowMenu);
    productRoot->bottomLayout()->addWidget(statusBar);

    if (m_globalWidgetRegistry) {
        if (QWidget* senderWidget = m_globalWidgetRegistry->createWidget(
                QStringLiteral("intermodule.sender"), productRoot)) {
            productRoot->topLayout()->addWidget(senderWidget, 0, Qt::AlignLeft | Qt::AlignVCenter);
        }
        productRoot->topLayout()->addStretch(1);
        if (QWidget* receiverWidget = m_globalWidgetRegistry->createWidget(
                QStringLiteral("intermodule.receiver"), productRoot)) {
            productRoot->topLayout()->addWidget(receiverWidget, 0, Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    for (const QString& moduleId : workflowSequence) {
        ModuleCoordinator* coordinator = appCoord->getModuleCoordinator(moduleId);
        if (!coordinator || !coordinator->getMainPage()) {
            continue;
        }

        productRoot->pageStack()->addWidget(coordinator->getMainPage());
        coordinator->getMainPage()->hide();
    }

    auto updateVisibleModule = [productRoot, appCoord](const QString& moduleId) {
        ModuleCoordinator* coordinator = appCoord->getModuleCoordinator(moduleId);
        if (!coordinator) {
            productRoot->showSupplementaryViews({});
            return;
        }

        if (QWidget* page = coordinator->getMainPage()) {
            productRoot->pageStack()->setCurrentWidget(page);
        }
        productRoot->showSupplementaryViews(coordinator->getSupplementaryViews());
    };

    QObject::connect(appCoord, &ApplicationCoordinator::currentModuleChanged,
                     workflowMenu, &ModuleNavigationModule::setCurrentModule);
    QObject::connect(appCoord, &ApplicationCoordinator::currentModuleChanged,
                     statusBar, &ModuleStatusBarModule::setCurrentModule);
    QObject::connect(appCoord, &ApplicationCoordinator::currentModuleChanged,
                     productRoot, updateVisibleModule);
    QObject::connect(appCoord, &ApplicationCoordinator::connectionStateChanged,
                     workflowMenu, &ModuleNavigationModule::setConnectionState);
    QObject::connect(appCoord, &ApplicationCoordinator::connectionStateChanged,
                     statusBar, &ModuleStatusBarModule::setConnectionState);
    QObject::connect(appCoord, &ApplicationCoordinator::healthSnapshotChanged,
                     statusBar, &ModuleStatusBarModule::setHealthSnapshot);
    QObject::connect(runtime, &LogicRuntime::logicNotification,
                     workflowMenu, &ModuleNavigationModule::onLogicNotification);

    productRoot->refreshVisibility();
    return productRoot;
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
