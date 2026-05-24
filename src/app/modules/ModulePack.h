#pragma once

#include <functional>

#include <QString>

class LogicRuntime;
struct ModuleUiAssemblyContext;

struct ModulePack
{
    QString moduleId;
    std::function<void(LogicRuntime*)> registerLogic;
    std::function<void(const ModuleUiAssemblyContext&)> registerUi;

    bool isValid() const
    {
        return !moduleId.isEmpty();
    }
};