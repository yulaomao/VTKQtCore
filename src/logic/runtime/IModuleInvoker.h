#pragma once

#include "contracts/ModuleInvoke.h"

class IModuleInvoker
{
public:
    virtual ~IModuleInvoker() = default;

    virtual ModuleInvokeResult invokeModule(const ModuleInvokeRequest& request) = 0;
};