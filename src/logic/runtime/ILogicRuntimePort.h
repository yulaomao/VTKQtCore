#pragma once

#include <QString>

#include "contracts/UiAction.h"

class ILogicRuntimePort
{
public:
    virtual ~ILogicRuntimePort() = default;

    virtual void sendAction(const UiAction& action) = 0;
    virtual void requestResync(const QString& reason) = 0;
};