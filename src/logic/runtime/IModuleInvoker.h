#pragma once

#include <QString>

#include "contracts/ModuleInvoke.h"

class IModuleInvoker
{
public:
    virtual ~IModuleInvoker() = default;

    virtual ModuleInvokeResult invokeModule(const ModuleInvokeRequest& request) = 0;
    virtual bool playPromptAudioPreset(const QString& presetId) = 0;
    virtual bool playPromptAudioSource(const QString& source) = 0;
    virtual bool registerPromptAudioPreset(const QString& presetId, const QString& source) = 0;
    virtual void stopPromptAudio() = 0;
};