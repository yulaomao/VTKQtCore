#include "KneePlaneModulePack.h"

#include "LogicRuntime.h"
#include "kneePlaneLogicHandler.h"
#include "app/modules/ModulePack.h"
#include "app/modules/ModulePackRegistry.h"

void registerKneePlaneModulePack()
{
    ModulePack modulePack;
    modulePack.moduleId = QStringLiteral("kneePlane");
    modulePack.registerLogic = [](LogicRuntime* runtime) {
        if (runtime) {
            runtime->registerModuleHandler(new KneePlaneLogicHandler(runtime));
        }
    };
    modulePack.registerUi = {};
    ModulePackRegistry::registerPack(modulePack);
}