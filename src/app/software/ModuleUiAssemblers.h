#pragma once

class MainWindow;
class LogicRuntime;
class ApplicationCoordinator;
class ILogicGateway;
class PageManager;
class GlobalUiManager;

struct ModuleUiAssemblyContext
{
    MainWindow* mainWindow = nullptr;
    LogicRuntime* runtime = nullptr;
    ApplicationCoordinator* applicationCoordinator = nullptr;
    ILogicGateway* gateway = nullptr;
    PageManager* pageManager = nullptr;
    GlobalUiManager* globalUiManager = nullptr;
};

void registerParamsModuleUi(const ModuleUiAssemblyContext& context);
void registerPointPickModuleUi(const ModuleUiAssemblyContext& context);
void registerPlanningModuleUi(const ModuleUiAssemblyContext& context);
void registerNavigationModuleUi(const ModuleUiAssemblyContext& context);
