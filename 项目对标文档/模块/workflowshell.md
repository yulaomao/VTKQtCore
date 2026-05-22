# workflowshell 模块规格

## 定位

workflowshell 不是业务模块，而是壳层能力集合，负责导航条、状态栏和壳层级信息展示。

## 组成

### ModuleNavigationModule

- 显示模块导航按钮
- 显示连接状态徽章
- 展示当前模块摘要与部分变换状态

### ModuleStatusBarModule

- 显示当前活跃模块
- 显示连接状态
- 展示健康快照摘要

## 主要职责

- 为所有业务模块提供统一导航入口
- 在壳层层面显示连接和健康状态
- 不承载业务数据处理，只负责展示和交互触发

## 典型流程

1. DefaultSoftwareInitializer 在 registerShellModules() 中创建壳层模块。
2. ModuleNavigationModule 接收模块显示顺序和 UiActionDispatcher。
3. 用户点击导航按钮时发送 switch_module 动作。
4. ModuleStatusBarModule 跟踪 currentModule、connectionState 和 healthSnapshot。

## 最小验证路径

1. 启动应用。
2. 检查右侧是否显示模块导航。
3. 点击任意模块按钮，确认页面切换正常。
4. 检查底部状态栏是否显示当前模块和连接状态。

## 源码索引

- [../../src/modules/workflowshell/ModuleNavigationModule.h](../../src/modules/workflowshell/ModuleNavigationModule.h)
- [../../src/modules/workflowshell/ModuleNavigationModule.cpp](../../src/modules/workflowshell/ModuleNavigationModule.cpp)
- [../../src/modules/workflowshell/ModuleStatusBarModule.h](../../src/modules/workflowshell/ModuleStatusBarModule.h)
- [../../src/modules/workflowshell/ModuleStatusBarModule.cpp](../../src/modules/workflowshell/ModuleStatusBarModule.cpp)
- [../../src/app/software/ConcreteSoftwareInitializers/DefaultSoftwareInitializer.cpp](../../src/app/software/ConcreteSoftwareInitializers/DefaultSoftwareInitializer.cpp)
