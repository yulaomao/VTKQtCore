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