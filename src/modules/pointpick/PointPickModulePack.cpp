#include "PointPickModulePack.h"

#include "PointPickModuleLogicHandler.h"
#include "PointPickPage.h"
#include "PointPickStatusPanel.h"
#include "ApplicationCoordinator.h"
#include "LogicRuntime.h"
#include "app/modules/ModulePack.h"
#include "app/modules/ModulePackRegistry.h"
#include "app/modules/ModuleUiAssemblyContext.h"
#include "app/modules/ModuleUiAssemblySupport.h"
#include "ui/coordination/ModuleCoordinator.h"

namespace {

void registerPointPickModuleUi(const ModuleUiAssemblyContext& context)
{
    if (!isModuleUiAssemblyContextValid(context)) {
        return;
    }

    auto* coordinator = new ModuleCoordinator(
        QStringLiteral("pointpick"),
        context.runtimePort,
        context.applicationCoordinator);
    auto* page = new PointPickPage();
    page->setActionDispatcher(coordinator->getActionDispatcher());
    auto* statusPanel = new PointPickStatusPanel(nullptr);
    coordinator->addSupplementaryView(statusPanel);
    coordinator->setMainPage(page);
    context.applicationCoordinator->registerModuleCoordinator(coordinator);

    QObject::connect(coordinator, &ModuleCoordinator::notificationForPage,
                     page, [page, statusPanel](const LogicNotification& notification) {
                         if (notification.eventType != LogicNotification::SceneNodesUpdated) {
                             return;
                         }

                         if (notification.payload.contains(QStringLiteral("points"))) {
                             page->updatePointList(
                                 notification.payload.value(QStringLiteral("points")).toStringList());
                         }

                         if (notification.payload.contains(QStringLiteral("pointCount"))) {
                             const int pointCount = notification.payload.value(
                                 QStringLiteral("pointCount")).toInt();
                             page->setPointCount(pointCount);
                             statusPanel->setPointCount(pointCount);
                         }

                         if (notification.payload.contains(QStringLiteral("confirmed"))) {
                             const bool confirmed = notification.payload.value(
                                 QStringLiteral("confirmed")).toBool();
                             page->setConfirmed(confirmed);
                             statusPanel->setConfirmed(confirmed);
                         }
                     });
}

}

void registerPointPickModulePack()
{
    ModulePack modulePack;
    modulePack.moduleId = QStringLiteral("pointpick");
    modulePack.registerLogic = [](LogicRuntime* runtime) {
        if (runtime) {
            runtime->registerModuleHandler(new PointPickModuleLogicHandler(runtime));
        }
    };
    modulePack.registerUi = [](const ModuleUiAssemblyContext& context) {
        registerPointPickModuleUi(context);
    };
    ModulePackRegistry::registerPack(modulePack);
}