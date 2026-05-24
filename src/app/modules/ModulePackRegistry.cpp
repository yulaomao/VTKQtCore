#include "ModulePackRegistry.h"

#include <QMap>

namespace {

QMap<QString, ModulePack>& registry()
{
    static QMap<QString, ModulePack> s_registry;
    return s_registry;
}

}

ModulePack ModulePackRegistry::findPack(const QString& moduleId)
{
    return registry().value(moduleId);
}

void ModulePackRegistry::registerPack(const ModulePack& modulePack)
{
    if (!modulePack.isValid()) {
        return;
    }

    registry().insert(modulePack.moduleId, modulePack);
}