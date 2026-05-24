#include "ParamsModulePack.h"

#include "ParamsModuleLogicHandler.h"
#include "ParamsPage.h"
#include "ApplicationCoordinator.h"
#include "LogicRuntime.h"
#include "app/modules/ModulePack.h"
#include "app/modules/ModulePackRegistry.h"
#include "app/modules/ModuleUiAssemblyContext.h"
#include "app/modules/ModuleUiAssemblySupport.h"
#include "ui/coordination/ModuleCoordinator.h"

#include <QLabel>

namespace {

void registerParamsModuleUi(const ModuleUiAssemblyContext& context)
{
    if (!isModuleUiAssemblyContextValid(context)) {
        return;
    }

    QLabel* summaryStatus = nullptr;
    auto* coordinator = new ModuleCoordinator(
        QStringLiteral("params"),
        context.runtimePort,
        context.applicationCoordinator);
    auto* page = new ParamsPage();
    page->setActionDispatcher(coordinator->getActionDispatcher());
    coordinator->addSupplementaryView(
        createModuleSummaryPanel(
            QStringLiteral("Parameters"),
            QStringLiteral("维护当前流程的参数有效性与数量概况。"),
            &summaryStatus,
            nullptr));
    coordinator->setMainPage(page);
    context.applicationCoordinator->registerModuleCoordinator(coordinator);

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

}

void registerParamsModulePack()
{
    ModulePack modulePack;
    modulePack.moduleId = QStringLiteral("params");
    modulePack.registerLogic = [](LogicRuntime* runtime) {
        if (runtime) {
            runtime->registerModuleHandler(new ParamsModuleLogicHandler(runtime));
        }
    };
    modulePack.registerUi = [](const ModuleUiAssemblyContext& context) {
        registerParamsModuleUi(context);
    };
    ModulePackRegistry::registerPack(modulePack);
}