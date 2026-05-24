#include "ReconstructionModulePack.h"

#include "ReconstructionModuleLogicHandler.h"
#include "ReconstructionPage.h"
#include "ApplicationCoordinator.h"
#include "LogicRuntime.h"
#include "app/modules/ModulePack.h"
#include "app/modules/ModulePackRegistry.h"
#include "app/modules/ModuleUiAssemblyContext.h"
#include "app/modules/ModuleUiAssemblySupport.h"
#include "ui/coordination/ModuleCoordinator.h"

#include <QLabel>

namespace {

void registerReconstructionModuleUi(const ModuleUiAssemblyContext& context)
{
    if (!isModuleUiAssemblyContextValid(context)) {
        return;
    }

    QLabel* summaryStatus = nullptr;
    auto* coordinator = new ModuleCoordinator(
        QStringLiteral("reconstruction"),
        context.runtimePort,
        context.applicationCoordinator);
    auto* page = new ReconstructionPage();
    page->setActionDispatcher(coordinator->getActionDispatcher());
    coordinator->addSupplementaryView(
        createModuleSummaryPanel(
            QStringLiteral("Reconstruction"),
            QStringLiteral("执行数据重建操作并显示重建状态。"),
            &summaryStatus,
            nullptr));
    coordinator->setMainPage(page);
    context.applicationCoordinator->registerModuleCoordinator(coordinator);

    QObject::connect(coordinator, &ModuleCoordinator::notificationForPage,
                     page, [page, summaryStatus](const LogicNotification& notification) {
                         if (notification.eventType != LogicNotification::StageChanged) {
                             return;
                         }

                         if (notification.payload.contains(QStringLiteral("status"))) {
                             const QString status = notification.payload.value(
                                 QStringLiteral("status")).toString();
                             const bool done = notification.payload.value(
                                 QStringLiteral("reconstructionDone"), false).toBool();
                             page->setReconstructionStatus(status, done);
                             if (summaryStatus) {
                                 summaryStatus->setText(
                                     QStringLiteral("重建状态: %1").arg(status));
                             }
                         }
                     });
}

}

void registerReconstructionModulePack()
{
    ModulePack modulePack;
    modulePack.moduleId = QStringLiteral("reconstruction");
    modulePack.registerLogic = [](LogicRuntime* runtime) {
        if (runtime) {
            runtime->registerModuleHandler(new ReconstructionModuleLogicHandler(runtime));
        }
    };
    modulePack.registerUi = [](const ModuleUiAssemblyContext& context) {
        registerReconstructionModuleUi(context);
    };
    ModulePackRegistry::registerPack(modulePack);
}