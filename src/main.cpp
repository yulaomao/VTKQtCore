#include "app/bootstrap/ApplicationLauncher.h"
#include "products/default/ProductDefinition.h"

int main(int argc, char* argv[])
{
    return runProductApplication(argc, argv, defaultProductDefinition());
}
