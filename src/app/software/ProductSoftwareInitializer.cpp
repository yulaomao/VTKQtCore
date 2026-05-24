#include "ProductSoftwareInitializer.h"

#include "app/bootstrap/ProductDefinition.h"
#include "app/modules/ModulePack.h"
#include "app/modules/ModulePackRegistry.h"
#include "app/modules/ModuleUiAssemblyContext.h"
#include "ApplicationCoordinator.h"
#include "LogicRuntime.h"
#include "MainWindow.h"
#include "communication/hub/CommunicationHub.h"
#include "logic/runtime/ILogicRuntimePort.h"
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
    const QVariantMap communication = profile.value(QStringLiteral("communication")).toMap();
    const QStringList channels = stringListFromVariant(
        communication.value(QStringLiteral("routingChannels")));
    return channels.isEmpty()
        ? QStringList{QStringLiteral("control.downstream")}
        : channels;
}

QString outboundControlChannelFromProfile(const QVariantMap& profile)
{
    const QVariantMap communication = profile.value(QStringLiteral("communication")).toMap();
    return stringFromVariantOrDefault(
        communication.value(QStringLiteral("controlPublishChannel")),
        QStringLiteral("control.upstream"));
}

QString ackChannelFromProfile(const QVariantMap& profile)
{
    const QVariantMap communication = profile.value(QStringLiteral("communication")).toMap();
    return stringFromVariantOrDefault(
        communication.value(QStringLiteral("ackChannel")),
        QStringLiteral("control.ack"));
}

QStringList productEnabledModules(const ProductDefinition& productDefinition,
                                  const QVariantMap& defaultProfile)
{
    const QStringList configured = stringListFromVariant(
        defaultProfile.value(QStringLiteral("enabledModules")));
    return configured.isEmpty() ? productDefinition.defaultEnabledModules : configured;
}

QStringList productModuleDisplayOrder(const ProductDefinition& productDefinition,
                                      const QVariantMap& defaultProfile)
{
    QStringList configured = stringListFromVariant(
        defaultProfile.value(QStringLiteral("moduleDisplayOrder")));
    if (configured.isEmpty()) {
        configured = stringListFromVariant(defaultProfile.value(QStringLiteral("workflowSequence")));
    }
    return configured.isEmpty() ? productDefinition.moduleDisplayOrder : configured;
}

QString productInitialModule(const ProductDefinition& productDefinition,
                             const QVariantMap& defaultProfile)
{
    const QString configured = defaultProfile.value(QStringLiteral("initialModule")).toString().trimmed();
    if (!configured.isEmpty()) {
        return configured;
    }
    if (!productDefinition.initialModule.trimmed().isEmpty()) {
        return productDefinition.initialModule;
    }

    const QStringList displayOrder = productModuleDisplayOrder(productDefinition, defaultProfile);
    return displayOrder.isEmpty() ? QString() : displayOrder.first();
}

QString initialConnectionStateName(RunMode mode)
{
    return mode == RunMode::Local
        ? QStringLiteral("Connected")
        : QStringLiteral("Disconnected");
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

void registerConfiguredModuleLogicPack(const QString& moduleId, LogicRuntime* runtime)
{
    const ModulePack modulePack = ModulePackRegistry::findPack(moduleId);
    if (modulePack.registerLogic) {
        modulePack.registerLogic(runtime);
        return;
    }

    qWarning().noquote() << QStringLiteral("[Product] missing logic module pack: %1").arg(moduleId);
}

void registerConfiguredModuleUiPack(const QString& moduleId, const ModuleUiAssemblyContext& context)
{
    const ModulePack modulePack = ModulePackRegistry::findPack(moduleId);
    if (modulePack.registerUi) {
        modulePack.registerUi(context);
        return;
    }

    qWarning().noquote() << QStringLiteral("[Product] missing UI module pack: %1").arg(moduleId);
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

    QStackedWidget* pageStack() const { return m_pageStack; }
    QHBoxLayout* topLayout() const { return m_topLayout; }
    QVBoxLayout* staticSideLayout() const { return m_staticSideLayout; }
    QHBoxLayout* bottomLayout() const { return m_bottomLayout; }

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

}

ProductSoftwareInitializer::ProductSoftwareInitializer(const ProductDefinition& productDefinition,
                                                       const QVariantMap& defaultProfile,
                                                       RunMode mode,
                                                       QObject* parent)
    : BaseSoftwareInitializer(productDefinition.softwareType, mode, parent)
    , m_productDefinition(productDefinition)
    , m_defaultProfile(defaultProfile)
{
}

QStringList ProductSoftwareInitializer::getEnabledModules() const
{
    return productEnabledModules(m_productDefinition, m_defaultProfile);
}

QStringList ProductSoftwareInitializer::getModuleDisplayOrder() const
{
    return productModuleDisplayOrder(m_productDefinition, m_defaultProfile);
}

QString ProductSoftwareInitializer::getInitialModule() const
{
    return productInitialModule(m_productDefinition, m_defaultProfile);
}

void ProductSoftwareInitializer::registerModuleLogicHandlers(LogicRuntime* runtime)
{
    if (!runtime) {
        return;
    }

    runtime->registerModuleHandler(new InterModuleSenderLogicHandler(runtime));
    runtime->registerModuleHandler(new InterModuleReceiverLogicHandler(runtime));

    for (const QString& moduleId : configuredEnabledModules()) {
        registerConfiguredModuleLogicPack(moduleId, runtime);
    }
}

void ProductSoftwareInitializer::registerModuleUIs(MainWindow* mainWindow,
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

    for (const QString& moduleId : configuredEnabledModules()) {
        registerConfiguredModuleUiPack(moduleId, context);
    }
}

void ProductSoftwareInitializer::registerGlobalWidgetFactories(
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

QWidget* ProductSoftwareInitializer::buildProductUi(MainWindow* mainWindow,
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

void ProductSoftwareInitializer::configureAdditionalSettings(LogicRuntime* runtime)
{
    Q_UNUSED(runtime);
}

void ProductSoftwareInitializer::registerCommunicationSources(CommunicationHub* commHub)
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
}