# UI 与产品装配交互规范

## 目标

本章定义页面层、模块协调层、产品级根界面和全局 UI 服务之间的职责边界。当前架构不再由基础框架预设固定 top/center/right/bottom 壳层；具体产品界面骨架由 SoftwareInitializer 自行构建。

## UI 分层原则

### 页面层

- 负责布局与控件交互。
- 不直接操作 SceneGraph。
- 不直接调用其他模块页面对象。
- 通过 UiActionDispatcher 发动作。
- 通过 LogicNotification 刷新显示。

### 模块协调层

- ModuleCoordinator 负责持有模块主页面、补充视图和模块生命周期信号。
- ApplicationCoordinator 负责维护当前模块状态、接收 LogicRuntime 通知，并把通知路由给对应模块协调器与全局 UI 服务。

### 产品装配层

- 具体 SoftwareInitializer 负责定义产品根 QWidget 树。
- 具体 SoftwareInitializer 决定如何挂载模块主页面、导航控件、补充视图和全局控件。
- 默认软件当前选择“顶部工具 + 中央页面栈 + 右侧产品面板 + 底部状态栏”的布局，但这是默认软件实现，不是框架冻结契约。

### 全局 UI 服务层

- MainWindow 负责根窗口、rootStack、overlay 宿主和 tool host。
- GlobalUiManager 负责通知、遮罩、错误提示和 VTK 窗口注册。
- GlobalWidgetRegistry 负责登记和创建全局控件工厂。

## MainWindow

MainWindow 当前负责：

- 持有 rootStack。
- 接收 initializer 提供的 workspaceRootWidget。
- 持有 globalOverlayLayer。
- 持有 globalToolHost。
- 持有 GlobalWidgetRegistry 指针入口。
- 通过 resizeEvent 同步 rootStack、overlay 和 tool host 的几何。

MainWindow 不再内建 WorkspaceShell，也不再预设模块页面宿主结构。

## 产品根界面

产品根界面由具体 SoftwareInitializer 通过 buildProductUi() 返回。该根界面至少应满足：

- 能够承载模块主页面。
- 能够响应 currentModuleChanged 等产品级状态变化。
- 能够挂载需要长期存在的产品控件。
- 能够决定补充视图如何随当前模块显示或隐藏。

默认软件当前的产品根界面选择了：

- 顶部：InterModuleSenderWidget 与 InterModuleReceiverWidget。
- 中央：QStackedWidget 形式的模块主页面区。
- 右侧：ModuleNavigationModule 与当前模块补充视图。
- 底部：ModuleStatusBarModule。

这些布局位置只是 DefaultSoftwareInitializer 的选择，不构成框架层稳定概念。

## GlobalWidgetRegistry

GlobalWidgetRegistry 是当前全局控件装配入口，职责包括：

- registerFactory(widgetId, factory)
- hasFactory(widgetId)
- createWidget(widgetId, parent)
- registeredWidgetIds()

约束如下：

- 框架只管理工厂，不预创建固定控件实例。
- 控件由 initializer 或其他调用方按需创建。
- 调用方通过 widgetId 获取控件，不依赖某个固定壳层挂载点。

默认软件当前登记的全局控件工厂包括：

- intermodule.sender
- intermodule.receiver

## ModuleCoordinator

ModuleCoordinator 当前职责包括：

- 持有模块主页面 QWidget。
- 持有模块私有 UiActionDispatcher。
- 维护模块补充视图列表。
- 在 activate()/deactivate() 时发出生命周期信号。
- 通过 notificationForPage 把通知转给页面。

### 视图约束

- 主页面通过 setMainPage() 登记。
- 额外补充视图通过 addSupplementaryView() 登记。
- ModuleCoordinator 不再暴露 Right/Bottom 等固定区域枚举。
- 补充视图最终挂载到哪里，由产品根界面自行决定。

### 页面约束

- 页面只从 coordinator->getActionDispatcher() 获取动作发送器。
- 页面不应自行创建直接指向 LogicRuntime 的引用。

## ApplicationCoordinator

ApplicationCoordinator 的核心职责为：

- registerModuleCoordinator()
- getModuleCoordinator(moduleId)
- getActionDispatcher() 获取产品级动作分发器
- getCurrentModule()
- onShellNotification(notification)

当前它不再承担以下职责：

- 不再切换固定中心页栈。
- 不再挂载右侧或底部辅助区。
- 不再依赖 WorkspaceShell。

### onShellNotification 的职责

- 识别 ModuleChanged / ActiveModuleChanged 并更新当前模块状态。
- 触发旧模块 deactivate() 与新模块 activate()。
- 把连接状态、错误和健康快照转给全局 UI 服务。
- 把 CurrentModule、AllModules、ModuleList 范围通知继续路由给对应模块协调器。

## UiActionDispatcher 使用规则

页面层发动作必须使用 UiActionDispatcher 提供的标准方法：

- sendCommand(command, payload)
- sendTargetedCommand(targetModule, command, payload)
- sendToTarget(targetName, command, payload)
- sendModuleUiEvent(targetModule, eventName, payload)
- requestModuleSwitch(targetModule)
- requestResync(reason)

禁止页面层做以下事情：

- 直接构造 socket JSON 并发送。
- 直接调用 LogicRuntime。
- 直接操作其他模块页面对象。

## 模块切换交互

当前模块切换链路如下：

1. 产品导航控件或页面入口通过 UiActionDispatcher 触发 requestModuleSwitch()。
2. LogicRuntime 校验目标模块并更新 ActiveModuleState。
3. LogicRuntime 发出 ModuleChanged 与 ActiveModuleChanged 通知。
4. ApplicationCoordinator 更新当前模块并驱动模块生命周期切换。
5. 产品根界面监听 currentModuleChanged，并自行切换主页面与补充视图显示。
6. 模块页面内的 VTK 窗口在 activated 后继续走 requestReconcile()。

切换后的显示结构由产品根界面决定，而不是由框架内建壳层决定。

## 全局 UI 行为

GlobalUiManager 仍负责以下能力：

- showNotification(message, level)
- hideNotification()
- showOverlay(message)
- hideOverlay()
- showConfirmation(title, message, callback)
- showError(errorCode, message, recoverable, suggestedAction)
- registerVtkWindow(window)

### 通知层行为

- 只有通知条时，overlay layer 可以保持透明鼠标穿透。
- 有遮罩时，overlay layer 应拦截交互。

这些能力是全局服务，不依赖固定壳层区域。

## 页面级职责边界

### DataGenPage

- 负责采集节点创建/编辑输入。
- 展示模块状态摘要。
- 承载 datagen_main 场景窗口。

### ParamsPage

- 负责参数表单录入与状态显示。

### PointPickPage

- 负责点列表和确认交互。

### PlanningPage

- 负责规划按钮、状态文本和双场景窗口承载。

### NavigationPage

- 负责导航启停、状态文本和位置显示。

## 页面级异常处理

页面不直接定义复杂错误恢复逻辑。推荐路径为：

1. 逻辑层形成 LogicNotification::ErrorOccurred 或其他明确错误通知。
2. ApplicationCoordinator / GlobalUiManager 决定是否弹错误提示。
3. 页面仅更新本地状态和只读提示，不接管全局恢复流程。

## 冻结项

- MainWindow、ApplicationCoordinator、ModuleCoordinator、UiActionDispatcher、GlobalUiManager、GlobalWidgetRegistry 这些公开类名。
- workspaceRootWidget、globalOverlayLayer、globalToolHost、supplementary view、global widget factory 这些装配概念。
- 固定壳层分区不再属于冻结项。

## 源码索引

- [../src/shell/MainWindow.h](../src/shell/MainWindow.h)
- [../src/shell/MainWindow.cpp](../src/shell/MainWindow.cpp)
- [../src/ui/coordination/ApplicationCoordinator.h](../src/ui/coordination/ApplicationCoordinator.h)
- [../src/ui/coordination/ApplicationCoordinator.cpp](../src/ui/coordination/ApplicationCoordinator.cpp)
- [../src/ui/coordination/ModuleCoordinator.h](../src/ui/coordination/ModuleCoordinator.h)
- [../src/ui/coordination/UiActionDispatcher.h](../src/ui/coordination/UiActionDispatcher.h)
- [../src/ui/globalui/GlobalUiManager.h](../src/ui/globalui/GlobalUiManager.h)
- [../src/ui/globalui/GlobalWidgetRegistry.h](../src/ui/globalui/GlobalWidgetRegistry.h)
- [../src/app/software/BaseSoftwareInitializer.h](../src/app/software/BaseSoftwareInitializer.h)
- [../src/app/software/ConcreteSoftwareInitializers/DefaultSoftwareInitializer.cpp](../src/app/software/ConcreteSoftwareInitializers/DefaultSoftwareInitializer.cpp)