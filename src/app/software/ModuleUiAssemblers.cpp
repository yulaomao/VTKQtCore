#include "ModuleUiAssemblers.h"

#include "MainWindow.h"
#include "LogicRuntime.h"
#include "ILogicGateway.h"
#include "PageManager.h"
#include "ApplicationCoordinator.h"
#include "GlobalUiManager.h"
#include "ui/coordination/ModuleCoordinator.h"

#include "ParamsPage.h"
#include "PointPickPage.h"
#include "PlanningPage.h"
#include "NavigationPage.h"
#include "ui/vtk3d/VtkSceneWindow.h"

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

namespace {

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

bool isContextValid(const ModuleUiAssemblyContext& context)
{
    return context.mainWindow && context.applicationCoordinator &&
           context.gateway && context.pageManager;
}

SceneGraph* sceneGraphFromContext(const ModuleUiAssemblyContext& context)
{
    return context.runtime ? context.runtime->getSceneGraph() : nullptr;
}

}

void registerParamsModuleUi(const ModuleUiAssemblyContext& context)
{
    if (!isContextValid(context)) {
        return;
    }

    QLabel* summaryStatus = nullptr;
    auto* coordinator = new ModuleCoordinator(
        QStringLiteral("params"),
        context.gateway,
        context.applicationCoordinator);
    auto* page = new ParamsPage();
    coordinator->addAuxiliaryWidget(
        createModuleSummaryPanel(
            QStringLiteral("Parameters"),
            QStringLiteral("维护当前流程的参数有效性与数量概况。"),
            &summaryStatus,
            context.mainWindow->getWorkspaceShell()),
        ModuleCoordinator::AuxiliaryRegion::Right);
    coordinator->setMainPage(page);
    context.pageManager->registerPage(QStringLiteral("params"), page);
    context.applicationCoordinator->registerModuleCoordinator(coordinator);

    QObject::connect(page, &ParamsPage::parameterApplied,
                     coordinator, [coordinator](const QVariantMap& params) {
                         for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
                             coordinator->sendModuleAction(
                                 UiAction::UpdateParameter,
                                 {{QStringLiteral("key"), it.key()},
                                  {QStringLiteral("value"), it.value()}});
                         }
                     });

    QObject::connect(coordinator, &ModuleCoordinator::notificationForPage,
                     page, [page, summaryStatus](const LogicNotification& notification) {
                         if (notification.eventType != LogicNotification::ButtonStateChanged) {
                             return;
                         }

                         const bool valid = notification.payload.value(
                             QStringLiteral("parametersValid"), false).toBool();
                         const int parameterCount = notification.payload.value(
                             QStringLiteral("parameterCount"), -1).toInt();
                         page->setParameterStatus(valid, parameterCount);
                         if (summaryStatus) {
                             summaryStatus->setText(
                                 QStringLiteral("参数状态: %1, 数量: %2")
                                     .arg(valid ? QStringLiteral("有效") : QStringLiteral("待检查"))
                                     .arg(parameterCount));
                         }
                     });
}

void registerPointPickModuleUi(const ModuleUiAssemblyContext& context)
{
    if (!isContextValid(context)) {
        return;
    }

    QLabel* summaryStatus = nullptr;
    auto* coordinator = new ModuleCoordinator(
        QStringLiteral("pointpick"),
        context.gateway,
        context.applicationCoordinator);
    auto* page = new PointPickPage();
    coordinator->addAuxiliaryWidget(
        createModuleSummaryPanel(
            QStringLiteral("Point Pick"),
            QStringLiteral("跟踪当前采点数量和确认状态。"),
            &summaryStatus,
            context.mainWindow->getWorkspaceShell()),
        ModuleCoordinator::AuxiliaryRegion::Right);
    coordinator->setMainPage(page);
    context.pageManager->registerPage(QStringLiteral("pointpick"), page);
    context.applicationCoordinator->registerModuleCoordinator(coordinator);

    QObject::connect(page, &PointPickPage::confirmPointsRequested,
                     coordinator, [coordinator]() {
                         coordinator->sendModuleAction(UiAction::ConfirmPoints);
                     });

    QObject::connect(coordinator, &ModuleCoordinator::notificationForPage,
                     page, [page, summaryStatus](const LogicNotification& notification) {
                         if (notification.eventType != LogicNotification::SceneNodesUpdated) {
                             return;
                         }

                         if (notification.payload.contains(QStringLiteral("pointCount"))) {
                             page->setPointCount(
                                 notification.payload.value(QStringLiteral("pointCount")).toInt());
                         }

                         if (notification.payload.contains(QStringLiteral("confirmed"))) {
                             page->setConfirmed(
                                 notification.payload.value(QStringLiteral("confirmed")).toBool());
                         }

                         if (summaryStatus) {
                             summaryStatus->setText(
                                 QStringLiteral("已选点数: %1, 已确认: %2")
                                     .arg(notification.payload.value(QStringLiteral("pointCount"), 0).toInt())
                                     .arg(notification.payload.value(QStringLiteral("confirmed"), false).toBool()
                                              ? QStringLiteral("是")
                                              : QStringLiteral("否")));
                         }
                     });
}

void registerPlanningModuleUi(const ModuleUiAssemblyContext& context)
{
    if (!isContextValid(context)) {
        return;
    }

    QLabel* summaryStatus = nullptr;
    auto* coordinator = new ModuleCoordinator(
        QStringLiteral("planning"),
        context.gateway,
        context.applicationCoordinator);
    auto* page = new PlanningPage();
    coordinator->addAuxiliaryWidget(
        createModuleSummaryPanel(
            QStringLiteral("Planning"),
            QStringLiteral("显示规划状态与当前 3D 视图工作摘要。"),
            &summaryStatus,
            context.mainWindow->getWorkspaceShell()),
        ModuleCoordinator::AuxiliaryRegion::Right);

    auto* planningWindow = new VtkSceneWindow(
        QStringLiteral("planning_main"),
        sceneGraphFromContext(context),
        page);
    auto* overviewWindow = new VtkSceneWindow(
        QStringLiteral("planning_overview"),
        sceneGraphFromContext(context),
        page);

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

    const double overviewPosition[3] = {0.0, -420.0, 180.0};
    const double overviewFocalPoint[3] = {0.0, 0.0, 18.0};
    const double overviewViewUp[3] = {0.0, 0.0, 1.0};
    const double overviewClippingRange[2] = {1.0, 4000.0};
    overviewWindow->setInitialCameraParams(
        overviewPosition,
        overviewFocalPoint,
        overviewViewUp,
        true,
        180.0,
        18.0,
        overviewClippingRange);

    page->setSceneWindow(planningWindow);
    page->setSecondarySceneWindow(overviewWindow);
    planningWindow->reconcile();
    overviewWindow->reconcile();

    if (context.globalUiManager) {
        context.globalUiManager->registerVtkWindow(planningWindow);
        context.globalUiManager->registerVtkWindow(overviewWindow);
    }

    coordinator->setMainPage(page);
    context.pageManager->registerPage(QStringLiteral("planning"), page);
    context.applicationCoordinator->registerModuleCoordinator(coordinator);

    QObject::connect(coordinator, &ModuleCoordinator::activated,
                     planningWindow, &VtkSceneWindow::requestReconcile);
    QObject::connect(coordinator, &ModuleCoordinator::activated,
                     overviewWindow, &VtkSceneWindow::requestReconcile);

    QObject::connect(page, &PlanningPage::generatePlanRequested,
                     coordinator, [coordinator]() {
                         coordinator->sendModuleAction(
                             UiAction::CustomAction,
                             {{QStringLiteral("subType"), QStringLiteral("generate_plan")}});
                     });

    QObject::connect(page, &PlanningPage::acceptPlanRequested,
                     coordinator, [coordinator]() {
                         coordinator->sendModuleAction(
                             UiAction::CustomAction,
                             {{QStringLiteral("subType"), QStringLiteral("accept_plan")}});
                     });

    QObject::connect(coordinator, &ModuleCoordinator::notificationForPage,
                     page, [page, summaryStatus](const LogicNotification& notification) {
                         if (!notification.payload.contains(QStringLiteral("status"))) {
                             return;
                         }

                         if (notification.eventType == LogicNotification::SceneNodesUpdated ||
                             notification.eventType == LogicNotification::StageChanged) {
                             const QString status = notification.payload.value(
                                 QStringLiteral("status")).toString();
                             page->setPlanStatus(status);
                             if (summaryStatus) {
                                 summaryStatus->setText(
                                     QStringLiteral("规划状态: %1").arg(status));
                             }
                         }
                     });
}

void registerNavigationModuleUi(const ModuleUiAssemblyContext& context)
{
    if (!isContextValid(context)) {
        return;
    }

    QLabel* summaryStatus = nullptr;
    auto* coordinator = new ModuleCoordinator(
        QStringLiteral("navigation"),
        context.gateway,
        context.applicationCoordinator);
    auto* page = new NavigationPage();
    coordinator->addAuxiliaryWidget(
        createModuleSummaryPanel(
            QStringLiteral("Navigation"),
            QStringLiteral("展示导航运行状态与实时位姿摘要。"),
            &summaryStatus,
            context.mainWindow->getWorkspaceShell()),
        ModuleCoordinator::AuxiliaryRegion::Right);

    auto* navigationWindow = new VtkSceneWindow(
        QStringLiteral("navigation_main"),
        sceneGraphFromContext(context),
        page);
    const double navigationPosition[3] = {300.0, 300.0, 220.0};
    const double navigationFocalPoint[3] = {0.0, 0.0, 0.0};
    const double navigationViewUp[3] = {0.0, 0.0, 1.0};
    const double navigationClippingRange[2] = {1.0, 4000.0};
    navigationWindow->setInitialCameraParams(
        navigationPosition,
        navigationFocalPoint,
        navigationViewUp,
        false,
        1.0,
        30.0,
        navigationClippingRange);
    page->setSceneWindow(navigationWindow);
    navigationWindow->reconcile();

    if (context.globalUiManager) {
        context.globalUiManager->registerVtkWindow(navigationWindow);
    }

    coordinator->setMainPage(page);
    context.pageManager->registerPage(QStringLiteral("navigation"), page);
    context.applicationCoordinator->registerModuleCoordinator(coordinator);

    QObject::connect(coordinator, &ModuleCoordinator::activated,
                     navigationWindow, &VtkSceneWindow::requestReconcile);

    QObject::connect(page, &NavigationPage::startNavigationRequested,
                     coordinator, [coordinator]() {
                         coordinator->sendModuleAction(UiAction::StartNavigation);
                     });

    QObject::connect(page, &NavigationPage::stopNavigationRequested,
                     coordinator, [coordinator]() {
                         coordinator->sendModuleAction(UiAction::StopNavigation);
                     });

    QObject::connect(coordinator, &ModuleCoordinator::notificationForPage,
                     page, [page, summaryStatus](const LogicNotification& notification) {
                         if (notification.eventType != LogicNotification::StageChanged) {
                             return;
                         }

                         if (notification.payload.contains(QStringLiteral("status"))) {
                             page->setNavigationStatus(
                                 notification.payload.value(QStringLiteral("status")).toString());
                         }

                         if (notification.payload.contains(QStringLiteral("navigating"))) {
                             page->setNavigating(
                                 notification.payload.value(QStringLiteral("navigating")).toBool());
                         }

                         if (notification.payload.contains(QStringLiteral("x")) &&
                             notification.payload.contains(QStringLiteral("y")) &&
                             notification.payload.contains(QStringLiteral("z"))) {
                             page->setCurrentPosition(
                                 notification.payload.value(QStringLiteral("x")).toDouble(),
                                 notification.payload.value(QStringLiteral("y")).toDouble(),
                                 notification.payload.value(QStringLiteral("z")).toDouble());
                         }

                         if (summaryStatus) {
                             summaryStatus->setText(
                                 QStringLiteral("导航状态: %1")
                                     .arg(notification.payload.value(
                                         QStringLiteral("status"),
                                         QStringLiteral("等待导航")).toString()));
                         }
                     });
}
