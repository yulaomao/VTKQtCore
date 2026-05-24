#pragma once

#include "app/bootstrap/ProductDefinition.h"
#include "modules/datagen/DataGenModulePack.h"
#include "modules/navigation/NavigationModulePack.h"
#include "modules/params/ParamsModulePack.h"
#include "modules/planning/PlanningModulePack.h"
#include "modules/pointpick/PointPickModulePack.h"

inline void registerDefaultProductModulePacks()
{
    registerDataGenModulePack();
    registerParamsModulePack();
    registerPointPickModulePack();
    registerPlanningModulePack();
    registerNavigationModulePack();
}

inline ProductDefinition defaultProductDefinition()
{
    ProductDefinition definition;
    definition.productId = QStringLiteral("vtkqtcore");
    definition.displayName = QStringLiteral("VTKQtCore");
    definition.softwareType = QStringLiteral("default");
    definition.styleTheme = QStringLiteral("clinical-light");
    definition.organizationName = QStringLiteral("VTKQtCore");
    definition.versionString = QStringLiteral("1.0.0");
    definition.defaultProfileResourcePath = QStringLiteral(":/products/default/product.json");
    definition.windowIconResourcePath = QStringLiteral(":/products/default/resources/app-window-icon.svg");
    definition.defaultEnabledModules = QStringList{
        QStringLiteral("datagen"),
        QStringLiteral("params"),
        QStringLiteral("pointpick"),
        QStringLiteral("planning"),
        QStringLiteral("navigation")
    };
    definition.moduleDisplayOrder = QStringList(definition.defaultEnabledModules);
    definition.initialModule = QStringLiteral("datagen");
    definition.registerModulePacks = registerDefaultProductModulePacks;
    return definition;
}