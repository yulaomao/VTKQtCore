#pragma once

class MainWindow;
class LogicRuntime;
class ApplicationCoordinator;
class ILogicRuntimePort;
class PageManager;
class GlobalUiManager;

struct ModuleUiAssemblyContext
{
    MainWindow* mainWindow = nullptr;
    LogicRuntime* runtime = nullptr;
    ApplicationCoordinator* applicationCoordinator = nullptr;
    ILogicRuntimePort* runtimePort = nullptr;
    PageManager* pageManager = nullptr;
    GlobalUiManager* globalUiManager = nullptr;
};

void registerParamsModuleUi(const ModuleUiAssemblyContext& context);
void registerDataGenModuleUi(const ModuleUiAssemblyContext& context);
void registerPointPickModuleUi(const ModuleUiAssemblyContext& context);
void registerPlanningModuleUi(const ModuleUiAssemblyContext& context);
void registerNavigationModuleUi(const ModuleUiAssemblyContext& context);
void registerReconstructionModuleUi(const ModuleUiAssemblyContext& context);
