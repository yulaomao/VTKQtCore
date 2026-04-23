#pragma once

#include <QString>

namespace PromptAudioPresetIds {

inline QString pollingProgress()
{
    return QStringLiteral("polling_progress");
}

inline QString pollingAttention()
{
    return QStringLiteral("polling_attention");
}

} // namespace PromptAudioPresetIds