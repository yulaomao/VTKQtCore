#pragma once

#include <QString>

namespace ReconstructionUiCommands {

inline QString startReconstruction()
{
    return QStringLiteral("start_reconstruction");
}

inline QString resetReconstruction()
{
    return QStringLiteral("reset_reconstruction");
}

} // namespace ReconstructionUiCommands
