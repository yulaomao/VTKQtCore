#pragma once

#include <QString>

namespace PlanningUiCommands {

inline QString generatePlan()
{
    return QStringLiteral("generate_plan");
}

inline QString acceptPlan()
{
    return QStringLiteral("accept_plan");
}

} // namespace PlanningUiCommands