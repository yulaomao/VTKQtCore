#pragma once

#include <QString>

namespace NavigationUiCommands {

inline QString startNavigation()
{
    return QStringLiteral("start_navigation");
}

inline QString stopNavigation()
{
    return QStringLiteral("stop_navigation");
}

} // namespace NavigationUiCommands