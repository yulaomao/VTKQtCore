#include "PlanningNavigationSoftwareInitializer.h"

#include "SoftwareInitializerFactory.h"

namespace {

const bool s_registeredPlanningNavigation = [] {
    SoftwareInitializerFactory::registerInitializer(
        QStringLiteral("planning_navigation"),
        [](const QString& softwareType, RunMode mode, QObject* parent) -> BaseSoftwareInitializer* {
            return new PlanningNavigationSoftwareInitializer(softwareType, mode, parent);
        });
    return true;
}();

}

PlanningNavigationSoftwareInitializer::PlanningNavigationSoftwareInitializer(
    const QString& softwareType,
    RunMode mode,
    QObject* parent)
    : DefaultSoftwareInitializer(softwareType, mode, parent)
{
}

QStringList PlanningNavigationSoftwareInitializer::getEnabledModules() const
{
    return {
        QStringLiteral("params"),
        QStringLiteral("planning"),
        QStringLiteral("navigation")
    };
}

QStringList PlanningNavigationSoftwareInitializer::getWorkflowSequence() const
{
    return {
        QStringLiteral("params"),
        QStringLiteral("planning"),
        QStringLiteral("navigation")
    };
}

QString PlanningNavigationSoftwareInitializer::getInitialModule() const
{
    return QStringLiteral("params");
}