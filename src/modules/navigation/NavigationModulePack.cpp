#include "NavigationModulePack.h"

#include "NavigationModuleLogicHandler.h"
#include "NavigationPage.h"
#include "ApplicationCoordinator.h"
#include "GlobalUiManager.h"
#include "LogicRuntime.h"
#include "app/modules/ModulePack.h"
#include "app/modules/ModulePackRegistry.h"
#include "app/modules/ModuleUiAssemblyContext.h"
#include "app/modules/ModuleUiAssemblySupport.h"
#include "ui/coordination/ModuleCoordinator.h"
#include "ui/vtk3d/VtkSceneWindow.h"

#include <QLabel>

namespace {

void registerNavigationModuleUi(const ModuleUiAssemblyContext& context)
{
    if (!isModuleUiAssemblyContextValid(context)) {
        return;
    }

    QLabel* summaryStatus = nullptr;
    auto* coordinator = new ModuleCoordinator(
        QStringLiteral("navigation"),
        context.runtimePort,
        context.applicationCoordinator);
    auto* page = new NavigationPage();
    page->setActionDispatcher(coordinator->getActionDispatcher());
    coordinator->addSupplementaryView(
        createModuleSummaryPanel(
            QStringLiteral("Navigation"),
            QStringLiteral("展示导航运行状态与实时位姿摘要。"),
            &summaryStatus,
            nullptr));

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
    context.applicationCoordinator->registerModuleCoordinator(coordinator);

    QObject::connect(coordinator, &ModuleCoordinator::activated,
                     navigationWindow, &VtkSceneWindow::requestReconcile);

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

}

void registerNavigationModulePack()
{
    ModulePack modulePack;
    modulePack.moduleId = QStringLiteral("navigation");
    modulePack.registerLogic = [](LogicRuntime* runtime) {
        if (runtime) {
            runtime->registerModuleHandler(new NavigationModuleLogicHandler(runtime));
        }
    };
    modulePack.registerUi = [](const ModuleUiAssemblyContext& context) {
        registerNavigationModuleUi(context);
    };
    ModulePackRegistry::registerPack(modulePack);
}