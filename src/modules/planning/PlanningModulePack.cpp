#include "PlanningModulePack.h"

#include "PlanningModuleLogicHandler.h"
#include "PlanningPage.h"
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

void registerPlanningModuleUi(const ModuleUiAssemblyContext& context)
{
    if (!isModuleUiAssemblyContextValid(context)) {
        return;
    }

    QLabel* summaryStatus = nullptr;
    auto* coordinator = new ModuleCoordinator(
        QStringLiteral("planning"),
        context.runtimePort,
        context.applicationCoordinator);
    auto* page = new PlanningPage();
    page->setActionDispatcher(coordinator->getActionDispatcher());
    coordinator->addSupplementaryView(
        createModuleSummaryPanel(
            QStringLiteral("Planning"),
            QStringLiteral("显示规划状态与当前 3D 视图工作摘要。"),
            &summaryStatus,
            nullptr));

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
    context.applicationCoordinator->registerModuleCoordinator(coordinator);

    QObject::connect(coordinator, &ModuleCoordinator::activated,
                     planningWindow, &VtkSceneWindow::requestReconcile);
    QObject::connect(coordinator, &ModuleCoordinator::activated,
                     overviewWindow, &VtkSceneWindow::requestReconcile);

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

}

void registerPlanningModulePack()
{
    ModulePack modulePack;
    modulePack.moduleId = QStringLiteral("planning");
    modulePack.registerLogic = [](LogicRuntime* runtime) {
        if (runtime) {
            runtime->registerModuleHandler(new PlanningModuleLogicHandler(runtime));
        }
    };
    modulePack.registerUi = [](const ModuleUiAssemblyContext& context) {
        registerPlanningModuleUi(context);
    };
    ModulePackRegistry::registerPack(modulePack);
}