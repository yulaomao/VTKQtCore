#pragma once

#include <QString>

class IPromptAudioService
{
public:
    virtual ~IPromptAudioService() = default;

    virtual bool playPreset(const QString& presetId) = 0;
    virtual bool playSource(const QString& source) = 0;
    virtual bool registerPreset(const QString& presetId, const QString& source) = 0;
    virtual void stopPlayback() = 0;
};