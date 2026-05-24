# 方案设计：UI去壳层化

当前 UI 框架已经完成去壳层化：框架层不再固化 WorkspaceShell、center/right/bottom/top 分区，也不再预设默认页面宿主。现在的稳定结构是由 MainWindow 提供中性根窗口，由 BaseSoftwareInitializer 定义装配骨架，由具体 SoftwareInitializer 通过 buildProductUi 构建产品根界面，再由模块向该界面贡献主页面、补充视图和全局控件。框架层只保留动作分发、运行时通信、模块注册与生命周期，以及按需创建的全局控件工厂注册表。

**需求摘要**
- 已确认目标：框架层不再预设 top、right、bottom、center 等布局概念，也不再预设默认页面宿主。
- 已确认目标：总体布局由具体 SoftwareInitializer 定义产品根界面骨架，模块再向该骨架贡献页面、补充视图或全局控件。
- 已确认目标：全局类控件继续存在，但不再由框架预置固定实例，而是由用户定义控件类，通过工厂按需创建，并在全局范围可获取、可调用。
- 已确认目标：框架层继续保留动作分发与运行时通信能力，以及模块注册与生命周期能力。
- 已确认约束：框架层不保留通用模块切换入口或默认导航 UI；切换逻辑由具体产品初始化器和产品层 UI 决定。
- 已确认约束：当前代码以已删除旧壳层为前提，不再回退到 WorkspaceShell 架构。
- 明确非目标：本次不在框架层继续维护固定全局控件集合。

**调研结论**
- 当前装配链已经完成解耦：BaseSoftwareInitializer 不再创建 PageManager 或 WorkspaceShell，而是创建 GlobalUiManager、GlobalWidgetRegistry、ApplicationCoordinator，并在注册模块 logic/UI 后通过 buildProductUi 获取产品根 QWidget 树。
- 当前 MainWindow 已经收缩为中性根窗口，只保留 workspaceRootWidget、overlay/tool host、full page 切换容器和全局控件注册表入口。
- 当前 ApplicationCoordinator 仍是有效的应用级协调器，但它承担的是模块协调器登记、模块切换广播、状态广播和逻辑通知转发，而不是固定壳层挂载中枢。
- 当前 ModuleCoordinator 已不再暴露固定区域语义，而是以主页面 + 补充视图集合的形式管理模块 UI 生命周期。
- 当前 DefaultSoftwareInitializer 通过 DefaultProductRoot 明确构建顶部条、中心页栈、右侧静态面板与补充视图、底部状态栏，并把导航模块、状态栏模块和全局控件工厂创建出的控件挂入该树。
- 当前规范文档 [项目对标文档/01-启动与装配规范.md](项目对标文档/01-启动与装配规范.md) 和 [项目对标文档/06-UI与Shell页面交互规范.md](项目对标文档/06-UI与Shell页面交互规范.md) 已经按去壳层化后的结构同步完成，可直接作为当前实现入口。

**推荐方案**
- 推荐方向：保持“框架负责能力与契约，初始化器负责产品根界面，模块负责自身视图与可选贡献物”的当前模型，不再重新引入固定壳层概念。
- 框架层保留四类稳定职责：
	- 第一，动作分发与运行时通信边界。继续保留 UiActionDispatcher 与 ILogicRuntimePort 这一类最小通信边界，页面和全局控件只通过该边界与 LogicRuntime 交互。
	- 第二，模块注册与生命周期。框架继续负责模块定义注册、模块实例创建、激活/停用生命周期回调，以及运行时级别的 moduleId 解析与消息投递。
	- 第三，全局控件工厂注册表。GlobalWidgetRegistry 只登记工厂，不预创建控件实例；任何模块或初始化器在需要时按控件 ID 获取并创建控件。
	- 第四，软件装配骨架。BaseSoftwareInitializer 只定义装配流程和扩展点，具体初始化器负责产品根 QWidget 树、全局控件工厂登记以及模块挂载策略。
- MainWindow 保持为应用根窗口，但不承载固定壳层语义；它的职责是容纳 workspaceRootWidget、overlay/tool host 和 full page 切换入口。
- ApplicationCoordinator 与 ModuleCoordinator 保持为当前协调层：前者负责应用级通知与模块状态广播，后者负责模块主页面、补充视图和通知落地。
- GlobalUiManager 保持为当前全局 UI 服务入口，负责通知、确认、遮罩、错误提示和 VTK 窗口登记；后续如需继续拆分，应以服务边界拆分，而不是恢复固定壳层。
- DefaultProductRoot 目前使用顶部、中心、右侧、底部布局只是默认产品实现，不代表框架层冻结这些区域；其他产品初始化器可以构建完全不同的 QWidget 树。

**当前实现要点**
1. BaseSoftwareInitializer::initialize 先创建 GlobalUiManager、GlobalWidgetRegistry、ApplicationCoordinator，再注册模块 logic、模块 UI 和全局控件工厂。
2. 具体 SoftwareInitializer 通过 buildProductUi 返回产品根 QWidget 树，MainWindow 通过 setWorkspaceRootWidget 挂入该界面。
3. ModuleUiAssemblers 负责为各模块创建主页面、VTK 视图和补充视图，并通过 ApplicationCoordinator/ModuleCoordinator 建立运行时与页面之间的关系。
4. DefaultSoftwareInitializer 当前在产品层显式创建 ModuleNavigationModule、ModuleStatusBarModule 和 GlobalWidgetRegistry 生成的全局控件，并把模块主页面加入中心页栈。
5. LogicRuntime 通过 initializeActiveModule 进入初始模块，ApplicationCoordinator 通过 currentModuleChanged 驱动产品根界面显示当前模块页面与补充视图。

**相关文件 / 模块**
- d:/code/C++/VTKQtCore/src/app/software/BaseSoftwareInitializer.h — 当前装配骨架，定义 registerGlobalWidgetFactories 和 buildProductUi 等关键扩展点。
- d:/code/C++/VTKQtCore/src/app/software/BaseSoftwareInitializer.cpp — 当前装配流程实现，负责创建 GlobalUiManager、GlobalWidgetRegistry、ApplicationCoordinator 并挂接产品根界面。
- d:/code/C++/VTKQtCore/src/app/software/ConcreteSoftwareInitializers/DefaultSoftwareInitializer.cpp — 当前默认产品根界面装配样板。
- d:/code/C++/VTKQtCore/src/app/software/ModuleUiAssemblers.h — 当前模块 UI 装配上下文与装配入口定义。
- d:/code/C++/VTKQtCore/src/app/software/ModuleUiAssemblers.cpp — 当前模块主页面、补充视图和 VTK 视图装配实现。
- d:/code/C++/VTKQtCore/src/shell/MainWindow.h — 当前应用根窗口与 workspaceRootWidget 宿主定义。
- d:/code/C++/VTKQtCore/src/shell/MainWindow.cpp — 当前根窗口、rootStack 和 overlay/tool host 装配实现。
- d:/code/C++/VTKQtCore/src/ui/coordination/ApplicationCoordinator.h — 当前应用级协调器定义。
- d:/code/C++/VTKQtCore/src/ui/coordination/ApplicationCoordinator.cpp — 当前模块切换广播、逻辑通知转发和全局 UI 协调实现。
- d:/code/C++/VTKQtCore/src/ui/coordination/ModuleCoordinator.h — 当前模块主页面、补充视图和生命周期协调契约。
- d:/code/C++/VTKQtCore/src/ui/coordination/ModuleCoordinator.cpp — 当前模块页面激活/停用与通知分发实现。
- d:/code/C++/VTKQtCore/src/ui/globalui/GlobalWidgetRegistry.h — 当前全局控件工厂注册表定义。
- d:/code/C++/VTKQtCore/src/ui/globalui/GlobalUiManager.h — 当前全局 UI 服务定义。
- d:/code/C++/VTKQtCore/src/logic/runtime/ILogicRuntimePort.h — UI 到运行时的最小稳定边界。
- d:/code/C++/VTKQtCore/src/logic/runtime/LogicRuntime.h — 当前运行时中心，承接动作、消息和模块路由。
- d:/code/C++/VTKQtCore/src/main.cpp — 当前启动顺序与初始化入口。
- d:/code/C++/VTKQtCore/项目对标文档/01-启动与装配规范.md — 当前启动与装配规范入口。
- d:/code/C++/VTKQtCore/项目对标文档/06-UI与Shell页面交互规范.md — 当前 UI 与产品根界面规范入口。

**验收标准**
- 启动主链路中不再存在对 WorkspaceShell、centerStack、rightShellHost、bottomShellHost 等固定壳层概念的依赖。
- BaseSoftwareInitializer 不在框架层创建默认壳层或默认页面宿主，而是通过 buildProductUi 接收具体产品根界面。
- 任一具体 SoftwareInitializer 都能独立定义产品根 QWidget 树，并决定模块视图、导航控件、全局控件的挂载方式。
- 框架层继续提供稳定的动作分发与运行时通信边界，页面和全局控件继续通过统一边界与 LogicRuntime 交互。
- 框架层继续提供模块注册、模块实例管理和生命周期调用能力，但不提供统一的默认导航或默认页面切换 UI。
- 全局控件通过工厂按需创建、全局访问，不依赖框架预置固定实例。
- 当前规范文档与代码保持一致，不再把固定壳层和默认中心页栈写成冻结事实。

**验证方式**
1. 启动验证：启动应用，确认 MainWindow 能正常显示由具体 initializer 构建的产品根界面。
2. 装配验证：检查至少一个具体 initializer 的 buildProductUi 实现，确认模块页面和产品级控件能够按初始化器定义成功挂载。
3. 运行时验证：在当前结构下触发页面动作，确认 UiActionDispatcher 到 LogicRuntime 的主链路有效。
4. 生命周期验证：激活、停用、切换模块时，确认 ModuleCoordinator 的主页面与补充视图显示正确，且不依赖固定壳层。
5. 全局控件验证：通过 GlobalWidgetRegistry 按需创建一个全局控件，确认初始化器和模块都能获取并使用它。
6. 文档验证：确认 [项目对标文档/01-启动与装配规范.md](项目对标文档/01-启动与装配规范.md) 与 [项目对标文档/06-UI与Shell页面交互规范.md](项目对标文档/06-UI与Shell页面交互规范.md) 的当前表述与实现一致。

**风险与缓解**
- 风险：不同产品初始化器可能各自实现不同布局，导致装配样板重复。缓解策略：在具体 initializer 之上提炼可复用 helper，但不把布局重新上收为框架预设。
- 风险：GlobalUiManager 仍然聚合了多种全局 UI 服务。缓解策略：后续按能力边界拆分服务对象，但保持当前对外装配方式稳定。
- 风险：默认产品根界面 objectName 与样式资源如果继续漂移，会造成主题层与产品层脱节。缓解策略：在产品根界面与样式表之间建立明确命名约定并定期回扫。

**决策与假设**
- 已确认：当前实现不再保留旧壳层前提。
- 已确认：框架层仅保留动作分发与运行时通信、模块注册与生命周期、全局控件工厂和装配骨架。
- 已确认：总体布局由具体 SoftwareInitializer 提供的产品根 QWidget 树决定。
- 已确认：全局控件通过工厂按需创建，而不是由框架预创建固定实例。
- 当前假设：MainWindow 继续作为应用根窗口，但不会重新承担固定壳层宿主职责。
- 当前假设：ILogicRuntimePort 和 LogicRuntime 主链路继续保留，不另起新的 UI 到逻辑通信模型。
- 当前假设：VTK 窗口注册继续由全局服务层负责，但该服务层不绑定固定 UI 壳层。

**待确认项**
- 无。当前文档已按现有代码结构同步，可直接作为实现参考。