#pragma once

#include <QString>

#include "ModulePack.h"

class ModulePackRegistry
{
public:
    static ModulePack findPack(const QString& moduleId);
    static void registerPack(const ModulePack& modulePack);
};