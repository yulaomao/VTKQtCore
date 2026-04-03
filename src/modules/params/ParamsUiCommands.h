#pragma once

#include <QString>

namespace ParamsUiCommands {

inline QString applyParameters()
{
    return QStringLiteral("apply_parameters");
}

inline QString updateParameter()
{
    return QStringLiteral("update_parameter");
}

} // namespace ParamsUiCommands