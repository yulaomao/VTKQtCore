#pragma once

#include "app/bootstrap/ProductDefinition.h"
#include "modules/navigation/NavigationModulePack.h"
#include "modules/params/ParamsModulePack.h"
#include "modules/planning/PlanningModulePack.h"

inline void registerPlanningNavigationProductModulePacks()
{
    registerParamsModulePack();
    registerPlanningModulePack();
    registerNavigationModulePack();
}

inline ProductDefinition planningNavigationProductDefinition()
{
    ProductDefinition definition;
    definition.productId = QStringLiteral("vtkqtcore_planning_navigation");
    definition.displayName = QStringLiteral("VTKQtCore Planning Navigation");
    definition.softwareType = QStringLiteral("planning_navigation");
    definition.styleTheme = QStringLiteral("clinical-light");
    definition.organizationName = QStringLiteral("VTKQtCore");
    definition.versionString = QStringLiteral("1.0.0");
    definition.defaultProfileResourcePath = QStringLiteral(":/products/planning_navigation/product.json");
    definition.windowIconResourcePath = QStringLiteral(":/products/planning_navigation/resources/app-window-icon.svg");
    definition.defaultEnabledModules = QStringList{
        QStringLiteral("params"),
        QStringLiteral("planning"),
        QStringLiteral("navigation")
    };
    definition.moduleDisplayOrder = QStringList(definition.defaultEnabledModules);
    definition.initialModule = QStringLiteral("params");
    definition.registerModulePacks = registerPlanningNavigationProductModulePacks;
    return definition;
}