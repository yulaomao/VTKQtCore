#include "app/bootstrap/ApplicationLauncher.h"
#include "products/planning_navigation/ProductDefinition.h"

int main(int argc, char* argv[])
{
    return runProductApplication(argc, argv, planningNavigationProductDefinition());
}