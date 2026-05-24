#include "DataGenModulePack.h"

#include "DataGenModuleLogicHandler.h"
#include "DataGenPage.h"
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

void registerDataGenModuleUi(const ModuleUiAssemblyContext& context)
{
    if (!isModuleUiAssemblyContextValid(context)) {
        return;
    }

    QLabel* summaryStatus = nullptr;
    auto* coordinator = new ModuleCoordinator(
        QStringLiteral("datagen"),
        context.runtimePort,
        context.applicationCoordinator);
    auto* page = new DataGenPage();
    page->setActionDispatcher(coordinator->getActionDispatcher());
    coordinator->addSupplementaryView(
        createModuleSummaryPanel(
            QStringLiteral("Data Generator"),
            QStringLiteral("创建 Point/Line/Model/Transform 节点，维护显示属性与父子变换。"),
            &summaryStatus,
            nullptr));

    auto* dataGenWindow = new VtkSceneWindow(
        QStringLiteral("datagen_main"),
        sceneGraphFromContext(context),
        page);
    const double cameraPosition[3] = {220.0, -260.0, 180.0};
    const double cameraFocalPoint[3] = {0.0, 0.0, 20.0};
    const double cameraViewUp[3] = {0.0, 0.0, 1.0};
    const double clippingRange[2] = {1.0, 4000.0};
    dataGenWindow->setInitialCameraParams(
        cameraPosition,
        cameraFocalPoint,
        cameraViewUp,
        false,
        1.0,
        30.0,
        clippingRange);
    page->setSceneWindow(dataGenWindow);
    dataGenWindow->reconcile();

    if (context.globalUiManager) {
        context.globalUiManager->registerVtkWindow(dataGenWindow);
    }

    coordinator->setMainPage(page);
    context.applicationCoordinator->registerModuleCoordinator(coordinator);

    QObject::connect(coordinator, &ModuleCoordinator::activated,
                     dataGenWindow, &VtkSceneWindow::requestReconcile);

    QObject::connect(coordinator, &ModuleCoordinator::notificationForPage,
                     page, [page, summaryStatus](const LogicNotification& notification) {
                         if (notification.eventType != LogicNotification::SceneNodesUpdated) {
                             return;
                         }

                         page->updateModuleState(notification.payload);
                         if (summaryStatus) {
                             const QVariantList nodeSummaries = notification.payload.value(
                                 QStringLiteral("nodeSummaries")).toList();
                             summaryStatus->setText(
                                 QStringLiteral("当前节点数: %1\n%2")
                                     .arg(nodeSummaries.size())
                                     .arg(notification.payload.value(
                                         QStringLiteral("statusText"),
                                         QStringLiteral("等待数据生成操作")).toString()));
                         }
                     });
}

}

void registerDataGenModulePack()
{
    ModulePack modulePack;
    modulePack.moduleId = QStringLiteral("datagen");
    modulePack.registerLogic = [](LogicRuntime* runtime) {
        if (runtime) {
            runtime->registerModuleHandler(new DataGenModuleLogicHandler(runtime));
        }
    };
    modulePack.registerUi = [](const ModuleUiAssemblyContext& context) {
        registerDataGenModuleUi(context);
    };
    ModulePackRegistry::registerPack(modulePack);
}