# UI 与 Shell 页面交互规范

## 目标

本章定义页面层、壳层和全局 UI 的职责边界，说明谁负责发动作、谁负责切页、谁负责全局提示，以及模块切换时页面与辅助区如何协同。

## UI 分层原则

### 页面层

- 负责布局与控件交互
- 不直接操作 SceneGraph
- 不直接调用其他模块 UI
- 通过 UiActionDispatcher 发动作
- 通过 LogicNotification 刷新显示

### 模块协调层

- ModuleCoordinator 负责页面与辅助部件生命周期
- ApplicationCoordinator 负责全局切换与全局通知路由

### 壳层与全局 UI

- WorkspaceShell 负责结构化布局
- GlobalUiManager 负责通知、遮罩、错误和 VTK 窗口注册
- MainWindow 负责根窗口、rootStack 和全局覆盖层

## MainWindow

MainWindow 是主窗口容器，负责：

- 持有 rootStack
- 持有 WorkspaceShell
- 持有 globalOverlayLayer
- 持有 globalToolHost
- 通过 resizeEvent 同步 overlay 和 tool host 几何

这意味着所有全局通知和遮罩都不直接挂在某个模块页面上，而是挂在 MainWindow 顶层。

## WorkspaceShell 分区

WorkspaceShell 暴露以下固定分区：

- topWidget
- centerStack
- rightWidget
- bottomWidget
- rightShellHost
- bottomShellHost

此外还具备模块级辅助区挂载能力：

- mountRightAuxiliary()
- clearRightAuxiliary()
- mountBottomAuxiliary()
- clearBottomAuxiliary()

### 语义约束

- ShellHost 用于跨模块长期存在的 UI，如导航和状态栏
- AuxiliaryHost 用于随当前模块切换的摘要面板或局部工具

## ModuleCoordinator

ModuleCoordinator 是单模块页面协调器，职责包括：

- 持有模块页面 QWidget
- 持有模块私有 UiActionDispatcher
- 维护右侧和底部辅助小部件列表
- 在 activate()/deactivate() 时发出生命周期信号
- 通过 notificationForPage 把通知转给页面

### 辅助区枚举

- AuxiliaryRegion::Right
- AuxiliaryRegion::Bottom

### 页面约束

- 页面只从 coordinator->getActionDispatcher() 获取动作发送器
- 页面不应自行创建直接指向 LogicRuntime 的引用

## ApplicationCoordinator

ApplicationCoordinator 的核心职责：

- registerModuleCoordinator()
- getModuleCoordinator(moduleId)
- getActionDispatcher() 获取全局动作分发器
- setCurrentModule(moduleId)
- onShellNotification(notification)

### onShellNotification 的职责

- 识别 ModuleChanged 并驱动切页
- 把 Shell 范围通知转给全局 UI 层
- 把 CurrentModule、AllModules、ModuleList 相关通知路由给对应模块协调器

## UiActionDispatcher 使用规则

页面层发动作必须使用 UiActionDispatcher 提供的标准方法：

- sendCommand(command, payload)
- sendTargetedCommand(targetModule, command, payload)
- sendToTarget(targetName, command, payload)
- sendModuleUiEvent(targetModule, eventName, payload)
- requestModuleSwitch(targetModule)
- requestResync(reason)

禁止页面层做以下事情：

- 直接构造 socket JSON 并发送
- 直接调用 LogicRuntime
- 直接操作其他模块页面对象

## 模块切换交互

模块切换的标准体验应满足：

1. 壳层导航按钮或其他入口触发 requestModuleSwitch()。
2. 运行时确认新模块有效并更新 ActiveModuleState。
3. ApplicationCoordinator 调用 setCurrentModule()。
4. 旧页面停用，旧辅助区清理。
5. 中央页面切换到新模块页面。
6. 新模块辅助区挂载。
7. 新模块 VTK 窗口接收 activated 并 requestReconcile()。

## 全局 UI 行为

GlobalUiManager 负责以下全局行为：

- showNotification(message, level)
- hideNotification()
- showOverlay(message)
- hideOverlay()
- showConfirmation(title, message, callback)
- showError(errorCode, message, recoverable, suggestedAction)
- registerVtkWindow(window)

### 通知层行为

- 只有通知条时，overlay layer 可以保持透明鼠标穿透
- 有遮罩时，overlay layer 应拦截交互

## 页面级职责边界

### DataGenPage

- 负责采集节点创建/编辑输入
- 展示模块状态摘要
- 承载 datagen_main 场景窗口

### ParamsPage

- 负责参数表单录入与状态显示

### PointPickPage

- 负责点列表和确认交互

### PlanningPage

- 负责规划按钮、状态文本和双场景窗口承载

### NavigationPage

- 负责导航启停、状态文本和位置显示

## 页面级异常处理

页面不直接定义复杂错误恢复逻辑。推荐路径为：

1. 逻辑层形成 LogicNotification::ErrorOccurred 或其他明确错误通知
2. ApplicationCoordinator / GlobalUiManager 决定是否弹错误提示
3. 页面仅更新本地状态和只读提示，不接管全局恢复流程

## 冻结项

- MainWindow、WorkspaceShell、ApplicationCoordinator、ModuleCoordinator、UiActionDispatcher、GlobalUiManager 这些公开类名
- topWidget、centerStack、rightWidget、bottomWidget、rightShellHost、bottomShellHost 这些布局概念
- requestModuleSwitch() 作为模块切换标准入口

## 源码索引

- [../src/shell/MainWindow.h](../src/shell/MainWindow.h)
- [../src/shell/MainWindow.cpp](../src/shell/MainWindow.cpp)
- [../src/shell/WorkspaceShell.h](../src/shell/WorkspaceShell.h)
- [../src/shell/WorkspaceShell.cpp](../src/shell/WorkspaceShell.cpp)
- [../src/ui/coordination/ApplicationCoordinator.h](../src/ui/coordination/ApplicationCoordinator.h)
- [../src/ui/coordination/ModuleCoordinator.h](../src/ui/coordination/ModuleCoordinator.h)
- [../src/ui/coordination/UiActionDispatcher.h](../src/ui/coordination/UiActionDispatcher.h)
- [../src/ui/globalui/GlobalUiManager.h](../src/ui/globalui/GlobalUiManager.h)

