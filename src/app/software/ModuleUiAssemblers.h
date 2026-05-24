#pragma once

class LogicRuntime;
class ApplicationCoordinator;
class ILogicRuntimePort;
class GlobalUiManager;
class GlobalWidgetRegistry;

struct ModuleUiAssemblyContext
{
    LogicRuntime* runtime = nullptr;
    ApplicationCoordinator* applicationCoordinator = nullptr;
    ILogicRuntimePort* runtimePort = nullptr;
    GlobalUiManager* globalUiManager = nullptr;
    GlobalWidgetRegistry* globalWidgetRegistry = nullptr;
};

void registerParamsModuleUi(const ModuleUiAssemblyContext& context);
void registerDataGenModuleUi(const ModuleUiAssemblyContext& context);
void registerPointPickModuleUi(const ModuleUiAssemblyContext& context);
void registerPlanningModuleUi(const ModuleUiAssemblyContext& context);
void registerNavigationModuleUi(const ModuleUiAssemblyContext& context);
void registerReconstructionModuleUi(const ModuleUiAssemblyContext& context);
