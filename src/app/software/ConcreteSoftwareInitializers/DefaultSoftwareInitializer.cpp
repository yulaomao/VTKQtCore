#include "DefaultSoftwareInitializer.h"
#include "SoftwareInitializerFactory.h"
#include "LogicRuntime.h"
#include "MainWindow.h"
#include "ApplicationCoordinator.h"
#include "communication/hub/CommunicationHub.h"
#include "communication/datasource/PollingTask.h"
#include "communication/datasource/SubscriptionSource.h"
#include "PageManager.h"
#include "ILogicGateway.h"
#include "ui/coordination/ModuleCoordinator.h"

#include "ParamsModuleLogicHandler.h"
#include "PointPickModuleLogicHandler.h"
#include "PlanningModuleLogicHandler.h"
#include "NavigationModuleLogicHandler.h"

#include "ParamsPage.h"
#include "PointPickPage.h"
#include "PlanningPage.h"
#include "NavigationPage.h"
#include "ui/vtk3d/VtkSceneWindow.h"

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

namespace {

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
    return communicationProfile(profile).value(
        QStringLiteral("controlPublishChannel")).toString(defaultControlPublishChannel());
}

QString ackChannelFromProfile(const QVariantMap& profile)
{
    return communicationProfile(profile).value(
        QStringLiteral("ackChannel")).toString(defaultAckChannel());
}

QString subscriptionChannelForModule(const QVariantMap& profile, const QString& moduleId)
{
    const QVariantMap channels = communicationProfile(profile).value(
        QStringLiteral("subscriptionChannels")).toMap();
    return channels.value(moduleId).toString(defaultSubscriptionChannel(moduleId));
}

QString pollingKeyForModule(const QVariantMap& profile, const QString& moduleId)
{
    const QVariantMap keys = communicationProfile(profile).value(
        QStringLiteral("pollingKeys")).toMap();
    return keys.value(moduleId).toString(defaultPollingKey(moduleId));
}

int pollingIntervalForModule(const QVariantMap& profile, const QString& moduleId)
{
    const QVariantMap intervals = communicationProfile(profile).value(
        QStringLiteral("pollingIntervalsMs")).toMap();
    const int interval = intervals.value(moduleId).toInt();
    return interval > 0 ? interval : 100;
}

double dispatchRateForModule(const QVariantMap& profile, const QString& moduleId)
{
    const QVariantMap rates = communicationProfile(profile).value(
        QStringLiteral("maxDispatchRateHz")).toMap();
    const double rate = rates.value(moduleId).toDouble();
    return rate > 0.0 ? rate : 30.0;
}

QWidget* createModuleSummaryPanel(const QString& title,
                                  const QString& description,
                                  QLabel** statusLabel,
                                  QWidget* parent)
{
    auto* panel = new QFrame(parent);
    panel->setFrameShape(QFrame::StyledPanel);

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(6);

    auto* titleLabel = new QLabel(title, panel);
    titleLabel->setStyleSheet(QStringLiteral("font-weight: 600;"));
    layout->addWidget(titleLabel);

    auto* descriptionLabel = new QLabel(description, panel);
    descriptionLabel->setWordWrap(true);
    layout->addWidget(descriptionLabel);

    *statusLabel = new QLabel(QStringLiteral("等待模块激活"), panel);
    (*statusLabel)->setWordWrap(true);
    layout->addWidget(*statusLabel);

    return panel;
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

DefaultSoftwareInitializer::DefaultSoftwareInitializer(const QString& softwareType, RunMode mode, QObject* parent)
    : BaseSoftwareInitializer(softwareType, mode, parent)
{
}

QStringList DefaultSoftwareInitializer::getEnabledModules() const
{
    return {
        QStringLiteral("params"),
        QStringLiteral("pointpick"),
        QStringLiteral("planning"),
        QStringLiteral("navigation")
    };
}

QStringList DefaultSoftwareInitializer::getWorkflowSequence() const
{
    return {
        QStringLiteral("params"),
        QStringLiteral("pointpick"),
        QStringLiteral("planning"),
        QStringLiteral("navigation")
    };
}

QString DefaultSoftwareInitializer::getInitialModule() const
{
    return QStringLiteral("params");
}

void DefaultSoftwareInitializer::registerModuleLogicHandlers(LogicRuntime* runtime)
{
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

void DefaultSoftwareInitializer::registerModuleUIs(MainWindow* mainWindow, LogicRuntime* runtime,
                                                   ApplicationCoordinator* appCoord,
                                                   ILogicGateway* gateway)
{
    QLabel* paramsSummaryStatus = nullptr;
    QLabel* pointPickSummaryStatus = nullptr;
    QLabel* planningSummaryStatus = nullptr;
    QLabel* navigationSummaryStatus = nullptr;

    // Params module
    auto* paramsCoord = new ModuleCoordinator(QStringLiteral("params"), gateway, appCoord);
    auto* paramsPage = new ParamsPage();
    paramsCoord->addAuxiliaryWidget(
        createModuleSummaryPanel(
            QStringLiteral("Parameters"),
            QStringLiteral("维护当前流程的参数有效性与数量概况。"),
            if (isModuleEnabled(QStringLiteral("params"))) {
                auto* paramsCoord = new ModuleCoordinator(QStringLiteral("params"), gateway, appCoord);
                auto* paramsPage = new ParamsPage();
                paramsCoord->addAuxiliaryWidget(
                    createModuleSummaryPanel(
                        QStringLiteral("Parameters"),
                        QStringLiteral("维护当前流程的参数有效性与数量概况。"),
                        &paramsSummaryStatus,
                        mainWindow->getWorkspaceShell()),
                    ModuleCoordinator::AuxiliaryRegion::Right);
                paramsCoord->setMainPage(paramsPage);
                m_pageManager->registerPage(QStringLiteral("params"), paramsPage);
                appCoord->registerModuleCoordinator(paramsCoord);

                connect(paramsPage, &ParamsPage::parameterApplied,
                        paramsCoord, [paramsCoord](const QVariantMap& params) {
                            for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
                                paramsCoord->sendModuleAction(
                                    UiAction::UpdateParameter,
                                    {{QStringLiteral("key"), it.key()},
                                     {QStringLiteral("value"), it.value()}});
                            }
                        });

                connect(paramsCoord, &ModuleCoordinator::notificationForPage,
                        paramsPage, [paramsPage, paramsSummaryStatus](const LogicNotification& notification) {
                            if (notification.eventType != LogicNotification::ButtonStateChanged) {
                                return;
                            }

                            const bool valid = notification.payload.value(
                                QStringLiteral("parametersValid"), false).toBool();
                            const int parameterCount = notification.payload.value(
                                QStringLiteral("parameterCount"), -1).toInt();
                            paramsPage->setParameterStatus(valid, parameterCount);
                            if (paramsSummaryStatus) {
                                paramsSummaryStatus->setText(
                                    QStringLiteral("参数状态: %1, 数量: %2")
                                        .arg(valid ? QStringLiteral("有效") : QStringLiteral("待检查"))
                                        .arg(parameterCount));
                            }
                        });
            }
            QStringLiteral("跟踪当前采点数量和确认状态。"),
            if (isModuleEnabled(QStringLiteral("pointpick"))) {
                auto* pointPickCoord = new ModuleCoordinator(QStringLiteral("pointpick"), gateway, appCoord);
                auto* pointPickPage = new PointPickPage();
                pointPickCoord->addAuxiliaryWidget(
                    createModuleSummaryPanel(
                        QStringLiteral("Point Pick"),
                        QStringLiteral("跟踪当前采点数量和确认状态。"),
                        &pointPickSummaryStatus,
                        mainWindow->getWorkspaceShell()),
                    ModuleCoordinator::AuxiliaryRegion::Right);
                pointPickCoord->setMainPage(pointPickPage);
                m_pageManager->registerPage(QStringLiteral("pointpick"), pointPickPage);
                appCoord->registerModuleCoordinator(pointPickCoord);

                connect(pointPickPage, &PointPickPage::confirmPointsRequested,
                        pointPickCoord, [pointPickCoord]() {
                            pointPickCoord->sendModuleAction(UiAction::ConfirmPoints);
                        });

                connect(pointPickCoord, &ModuleCoordinator::notificationForPage,
                        pointPickPage, [pointPickPage, pointPickSummaryStatus](const LogicNotification& notification) {
                            if (notification.eventType != LogicNotification::SceneNodesUpdated) {
                                return;
                            }

                            if (notification.payload.contains(QStringLiteral("pointCount"))) {
                                pointPickPage->setPointCount(
                                    notification.payload.value(QStringLiteral("pointCount")).toInt());
                            }

                            if (notification.payload.contains(QStringLiteral("confirmed"))) {
                                pointPickPage->setConfirmed(
                                    notification.payload.value(QStringLiteral("confirmed")).toBool());
                            }

                            if (pointPickSummaryStatus) {
                                pointPickSummaryStatus->setText(
                                    QStringLiteral("已选点数: %1, 已确认: %2")
                                        .arg(notification.payload.value(QStringLiteral("pointCount"), 0).toInt())
                                        .arg(notification.payload.value(QStringLiteral("confirmed"), false).toBool()
                                                 ? QStringLiteral("是")
                                                 : QStringLiteral("否")));
                            }
                        });
            }
            QStringLiteral("显示规划状态与当前 3D 视图工作摘要。"),
            if (isModuleEnabled(QStringLiteral("planning"))) {
                auto* planningCoord = new ModuleCoordinator(QStringLiteral("planning"), gateway, appCoord);
                auto* planningPage = new PlanningPage();
                planningCoord->addAuxiliaryWidget(
                    createModuleSummaryPanel(
                        QStringLiteral("Planning"),
                        QStringLiteral("显示规划状态与当前 3D 视图工作摘要。"),
                        &planningSummaryStatus,
                        mainWindow->getWorkspaceShell()),
                    ModuleCoordinator::AuxiliaryRegion::Right);
                auto* planningWindow = new VtkSceneWindow(
                    QStringLiteral("planning_main"),
                    runtime ? runtime->getSceneGraph() : nullptr,
                    planningPage);
                auto* planningOverviewWindow = new VtkSceneWindow(
                    QStringLiteral("planning_overview"),
                    runtime ? runtime->getSceneGraph() : nullptr,
                    planningPage);
                const double planningPosition[3] = {260.0, 220.0, 180.0};
                const double planningFocalPoint[3] = {0.0, 0.0, 18.0};
                const double planningViewUp[3] = {0.0, 0.0, 1.0};
                const double planningClippingRange[2] = {1.0, 3000.0};
                planningWindow->setInitialCameraParams(
                    planningPosition,
                    planningFocalPoint,
                    planningViewUp,
                    false,
                    1.0,
                    30.0,
                    planningClippingRange);
                const double planningOverviewPosition[3] = {0.0, -420.0, 180.0};
                const double planningOverviewFocalPoint[3] = {0.0, 0.0, 18.0};
                const double planningOverviewViewUp[3] = {0.0, 0.0, 1.0};
                const double planningOverviewClippingRange[2] = {1.0, 4000.0};
                planningOverviewWindow->setInitialCameraParams(
                    planningOverviewPosition,
                    planningOverviewFocalPoint,
                    planningOverviewViewUp,
                    true,
                    180.0,
                    18.0,
                    planningOverviewClippingRange);
                planningPage->setSceneWindow(planningWindow);
                planningPage->setSecondarySceneWindow(planningOverviewWindow);
                planningWindow->reconcile();
                planningOverviewWindow->reconcile();

                if (m_globalUiManager) {
                    m_globalUiManager->registerVtkWindow(planningWindow);
                    m_globalUiManager->registerVtkWindow(planningOverviewWindow);
                }

                planningCoord->setMainPage(planningPage);
                m_pageManager->registerPage(QStringLiteral("planning"), planningPage);
                appCoord->registerModuleCoordinator(planningCoord);

                connect(planningCoord, &ModuleCoordinator::activated,
                        planningWindow, &VtkSceneWindow::requestReconcile);
                connect(planningCoord, &ModuleCoordinator::activated,
                        planningOverviewWindow, &VtkSceneWindow::requestReconcile);

                connect(planningPage, &PlanningPage::generatePlanRequested,
                        planningCoord, [planningCoord]() {
                            planningCoord->sendModuleAction(
                                UiAction::CustomAction,
                                {{QStringLiteral("subType"), QStringLiteral("generate_plan")}});
                        });

                connect(planningPage, &PlanningPage::acceptPlanRequested,
                        planningCoord, [planningCoord]() {
                            planningCoord->sendModuleAction(
                                UiAction::CustomAction,
                                {{QStringLiteral("subType"), QStringLiteral("accept_plan")}});
                        });

                connect(planningCoord, &ModuleCoordinator::notificationForPage,
                        planningPage, [planningPage, planningSummaryStatus](const LogicNotification& notification) {
                            if (!notification.payload.contains(QStringLiteral("status"))) {
                                return;
                            }

                            if (notification.eventType == LogicNotification::SceneNodesUpdated ||
                                notification.eventType == LogicNotification::StageChanged) {
                                planningPage->setPlanStatus(
                                    notification.payload.value(QStringLiteral("status")).toString());
                                if (planningSummaryStatus) {
                                    planningSummaryStatus->setText(
                                        QStringLiteral("规划状态: %1")
                                            .arg(notification.payload.value(QStringLiteral("status")).toString()));
                                }
                            }
                        });
            }
            QStringLiteral("展示导航运行状态与实时位姿摘要。"),
            if (isModuleEnabled(QStringLiteral("navigation"))) {
                auto* navigationCoord = new ModuleCoordinator(QStringLiteral("navigation"), gateway, appCoord);
                auto* navigationPage = new NavigationPage();
                navigationCoord->addAuxiliaryWidget(
                    createModuleSummaryPanel(
                        QStringLiteral("Navigation"),
                        QStringLiteral("展示导航运行状态与实时位姿摘要。"),
                        &navigationSummaryStatus,
                        mainWindow->getWorkspaceShell()),
                    ModuleCoordinator::AuxiliaryRegion::Right);
                auto* navigationWindow = new VtkSceneWindow(
                    QStringLiteral("navigation_main"),
                    runtime ? runtime->getSceneGraph() : nullptr,
                    navigationPage);
                const double navPosition[3] = {300.0, 300.0, 220.0};
                const double navFocalPoint[3] = {0.0, 0.0, 0.0};
                const double navViewUp[3] = {0.0, 0.0, 1.0};
                const double navClippingRange[2] = {1.0, 4000.0};
                navigationWindow->setInitialCameraParams(
                    navPosition,
                    navFocalPoint,
                    navViewUp,
                    false,
                    1.0,
                    30.0,
                    navClippingRange);
                navigationPage->setSceneWindow(navigationWindow);
                navigationWindow->reconcile();

                if (m_globalUiManager) {
                    m_globalUiManager->registerVtkWindow(navigationWindow);
                }

                navigationCoord->setMainPage(navigationPage);
                m_pageManager->registerPage(QStringLiteral("navigation"), navigationPage);
                appCoord->registerModuleCoordinator(navigationCoord);

                connect(navigationCoord, &ModuleCoordinator::activated,
                        navigationWindow, &VtkSceneWindow::requestReconcile);

                connect(navigationPage, &NavigationPage::startNavigationRequested,
                        navigationCoord, [navigationCoord]() {
                            navigationCoord->sendModuleAction(UiAction::StartNavigation);
                        });

                connect(navigationPage, &NavigationPage::stopNavigationRequested,
                        navigationCoord, [navigationCoord]() {
                            navigationCoord->sendModuleAction(UiAction::StopNavigation);
                        });

                connect(navigationCoord, &ModuleCoordinator::notificationForPage,
                        navigationPage, [navigationPage, navigationSummaryStatus](const LogicNotification& notification) {
                            if (notification.eventType != LogicNotification::StageChanged) {
                                return;
                            }

                            if (notification.payload.contains(QStringLiteral("status"))) {
                                navigationPage->setNavigationStatus(
                                    notification.payload.value(QStringLiteral("status")).toString());
                            }

                            if (notification.payload.contains(QStringLiteral("navigating"))) {
                                navigationPage->setNavigating(
                                    notification.payload.value(QStringLiteral("navigating")).toBool());
                            }

                            if (notification.payload.contains(QStringLiteral("x")) &&
                                notification.payload.contains(QStringLiteral("y")) &&
                                notification.payload.contains(QStringLiteral("z"))) {
                                navigationPage->setCurrentPosition(
                                    notification.payload.value(QStringLiteral("x")).toDouble(),
                                    notification.payload.value(QStringLiteral("y")).toDouble(),
                                    notification.payload.value(QStringLiteral("z")).toDouble());
                            }

                            if (navigationSummaryStatus) {
                                navigationSummaryStatus->setText(
                                    QStringLiteral("导航状态: %1")
                                        .arg(notification.payload.value(
                                            QStringLiteral("status"),
                                            QStringLiteral("等待导航")).toString()));
                            }
                        });
            }
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

            const QStringList modules = configuredEnabledModules();
            for (const QString& moduleId : modules) {
                const QString channel = subscriptionChannelForModule(profile, moduleId);
                if (!channel.isEmpty()) {
                    commHub->addSubscriptionSource(new SubscriptionSource(
                        QStringLiteral("%1_subscription").arg(moduleId),
                        channel,
                        moduleId));
                }

                const QString pollingKey = pollingKeyForModule(profile, moduleId);
                if (!pollingKey.isEmpty()) {
                    auto* task = new PollingTask(
                        QStringLiteral("%1_poll").arg(moduleId),
                        moduleId,
                        pollingKey,
                        pollingIntervalForModule(profile, moduleId));
                    task->setLatestWins(true);
                    task->setChangeDetection(true);
                    task->setMaxDispatchRateHz(dispatchRateForModule(profile, moduleId));
                    commHub->addPollingTask(task);
                }
            }
