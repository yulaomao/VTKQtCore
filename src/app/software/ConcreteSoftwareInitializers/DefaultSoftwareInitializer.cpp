#include "DefaultSoftwareInitializer.h"
#include "SoftwareInitializerFactory.h"
#include "LogicRuntime.h"
#include "MainWindow.h"
#include "ApplicationCoordinator.h"
#include "PageManager.h"
#include "ILogicGateway.h"

#include "ParamsModuleLogicHandler.h"
#include "PointPickModuleLogicHandler.h"
#include "PlanningModuleLogicHandler.h"
#include "NavigationModuleLogicHandler.h"

#include "ParamsPage.h"
#include "PointPickPage.h"
#include "PlanningPage.h"
#include "NavigationPage.h"

#include "ParamsModuleCoordinator.h"
#include "PointPickModuleCoordinator.h"
#include "PlanningModuleCoordinator.h"
#include "NavigationModuleCoordinator.h"

namespace {
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
    runtime->registerModuleHandler(new ParamsModuleLogicHandler(runtime));
    runtime->registerModuleHandler(new PointPickModuleLogicHandler(runtime));
    runtime->registerModuleHandler(new PlanningModuleLogicHandler(runtime));
    runtime->registerModuleHandler(new NavigationModuleLogicHandler(runtime));
}

void DefaultSoftwareInitializer::registerModuleUIs(MainWindow* mainWindow, ApplicationCoordinator* appCoord,
                                                   ILogicGateway* gateway)
{
    Q_UNUSED(mainWindow);

    // Params module
    auto* paramsCoord = new ParamsModuleCoordinator(QStringLiteral("params"), gateway, appCoord);
    auto* paramsPage = new ParamsPage();
    paramsCoord->setMainPage(paramsPage);
    m_pageManager->registerPage(QStringLiteral("params"), paramsPage);
    appCoord->registerModuleCoordinator(paramsCoord);

    // PointPick module
    auto* pointPickCoord = new PointPickModuleCoordinator(QStringLiteral("pointpick"), gateway, appCoord);
    auto* pointPickPage = new PointPickPage();
    pointPickCoord->setMainPage(pointPickPage);
    m_pageManager->registerPage(QStringLiteral("pointpick"), pointPickPage);
    appCoord->registerModuleCoordinator(pointPickCoord);

    // Planning module
    auto* planningCoord = new PlanningModuleCoordinator(QStringLiteral("planning"), gateway, appCoord);
    auto* planningPage = new PlanningPage();
    planningCoord->setMainPage(planningPage);
    m_pageManager->registerPage(QStringLiteral("planning"), planningPage);
    appCoord->registerModuleCoordinator(planningCoord);

    // Navigation module
    auto* navigationCoord = new NavigationModuleCoordinator(QStringLiteral("navigation"), gateway, appCoord);
    auto* navigationPage = new NavigationPage();
    navigationCoord->setMainPage(navigationPage);
    m_pageManager->registerPage(QStringLiteral("navigation"), navigationPage);
    appCoord->registerModuleCoordinator(navigationCoord);
}
