## 方案设计：基础UI框架简化重构

本次重构的目标不是继续扩展现有通信中枢，而是把当前项目收敛成一套更接近“空白基础 UI 框架”的结构：由一个明确的 UI 管理中心注册和切换模块页面，各模块 logic 只处理输入消息、更新 SceneGraph 或发出 UI 刷新通知。推荐方案是保留当前已经成熟的 UI 注册与页面协调骨架，删除或降级通信兼容主链路中的冗余层，把本地主链路收缩为“Page -> Dispatcher -> Runtime -> ModuleLogic -> SceneGraph/Notification”。这样既能显著降低复杂度，也不会把现有模块页面和逻辑体系推翻重来。

**实施状态（2026-05-22）**
- 已完成阶段 1 到阶段 5 的首轮落地。
- 已新增 `src/logic/runtime/ILogicRuntimePort.h`，并由 `LogicRuntime` 实现该最小边界。
- 已把 `UiActionDispatcher`、`ApplicationCoordinator`、`ModuleCoordinator`、`ModuleUiAssemblers`、`BaseSoftwareInitializer`、`DefaultSoftwareInitializer` 切换到 `ILogicRuntimePort`。
- 已把 `ApplicationCoordinator`、`ModuleNavigationModule`、`InterModuleReceiverWidget`、`ModuleUiEventBinding` 的通知来源切到 `LogicRuntime::logicNotification`。
- 已把 `LogicRuntime` 的 action/state 路由内联，不再依赖 `AppMessageCenter`。
- 已删除 `ILogicGateway`、`LocalLogicGateway`、`AppMessageCenter` 源码文件。
- 已把 `CommunicationHub` 收缩为 socket 模式可选适配层；本地模式下不再创建通信对象。
- 已补回 socket 模式的出站兼容：UI dispatcher 的 action/resync 会在 socket 模式镜像发送到 `CommunicationHub`，但本地 UI 主链路不依赖 loopback。

**需求摘要**
- 已确认目标：构建一个简化版基础 UI 框架，能够集中注册各模块 page，并按软件初始化策略自由组合 UI。
- 已确认目标：模块 logic 负责根据传入消息更新节点或 UI 显示，UI 本身不承担复杂通信职责。
- 已确认目标：保留当前项目中已验证有效的模块装配、页面切换、SceneGraph 和 LogicNotification 机制。
- 已确认目标：给出明确的保留类、删除类、替代关系和迁移步骤，便于后续直接实施。
- 明确非目标：本次不重写业务模块逻辑，不重做 SceneGraph，不把所有模块 page 改造成全新开发模式。
- 明确非目标：本次不要求删除所有 socket 相关能力，而是把它们从基础框架主链路上降级为可选外部适配层。

**调研结论**
- 当前 UI 装配层并不重，甚至已经接近目标形态。BaseSoftwareInitializer、ModuleUiAssemblers、ApplicationCoordinator、ModuleCoordinator、PageManager、WorkspaceShell 这一组对象已经天然具备“UI 管理中心 + 模块注册式页面框架”的结构。
- 当前复杂度主要来自 UI 动作进入 logic 前的兼容链路：UiActionDispatcher 依赖 ILogicGateway，ILogicGateway 由 LocalLogicGateway 实现，LocalLogicGateway 在本地和 socket 模式之间分流，并可进一步把消息送入 CommunicationHub 的 loopback 流程。
- 当前 LogicRuntime 又通过 AppMessageCenter 转一次通用消息总线，之后才交给 ModuleLogicRegistry 和 ModuleLogicHandler。对于目标中的本地基础框架，这一层过度通用了。
- MessageRouter、LegacySocketAdapter、CommunicationHub 的 loopback、健康状态、多类别 envelope 处理，主要是为了兼容旧 socket 协议和外部通信场景，而不是为了支持基础 UI 框架本身。
- 当前 ApplicationCoordinator 其实已经在做“UI 管理中心”的工作：注册模块协调器、切换页面、挂载辅助区、接收并转发 LogicNotification。它不需要再承担通信桥接角色。

**推荐方案**
- 推荐方向：把当前架构显式切成两部分。
- 第一部分是基础 UI 框架核心，始终存在，负责页面注册、模块切换、动作分发、logic 路由、SceneGraph 更新和 UI 刷新。
- 第二部分是外部通信适配层，只在需要 socket、旧协议或外部消息源时挂接，不再作为本地主链路必经层。
- 推荐的基础 UI 框架目标结构如下：
- UI Host Layer：MainWindow、WorkspaceShell、可选 GlobalUiManager。
- UI Coordination Layer：BaseSoftwareInitializer、ApplicationCoordinator、ModuleCoordinator、PageManager、ModuleUiAssemblers。
- Logic Runtime Layer：LogicRuntime、ModuleLogicRegistry、ModuleLogicHandler、SceneGraph。
- Optional Communication Adapter Layer：CommunicationHub、LegacySocketAdapter、MessageRouter，以及后续任何外部协议接入类。
- 推荐保留 ApplicationCoordinator 作为正式的 UI 管理中心，不再引入新的“大而全 UI 中枢”类。原因是它已经承担了当前最核心的页面注册、切换和通知分发职责。
- 推荐保留 ModuleCoordinator，而不是把它并回 ApplicationCoordinator。原因是模块页面生命周期、模块私有 dispatcher、模块辅助区管理天然属于模块边界，不应重新耦合到全局协调器。
- 推荐保留 PageManager，而不是立即并入 ApplicationCoordinator。原因是它足够简单，且“页面栈管理”与“通知协调”职责边界清晰，目前不是复杂度主要来源。
- 推荐保留 ModuleUiAssemblers。原因是它已经是模块 UI 装配的稳定入口，复杂度低，且与 SoftwareInitializer 的装配模型匹配良好。
- 推荐保留 LogicRuntime、ModuleLogicRegistry、ModuleLogicHandler、SceneGraph、LogicNotification 这一组对象，因为它们正好对应“logic 根据消息更新节点或 UI 显示”的目标。
- 推荐删除 ILogicGateway 和 LocalLogicGateway 在基础框架中的主链路角色。它们的存在价值主要来自本地/远端双路径判断，而不是页面管理或逻辑分发本身。
- 推荐删除 AppMessageCenter 作为基础框架默认总线。对于本地框架，更合适的主链路是 LogicRuntime 直接完成模块路由，而不是所有 action 都先转换成 AppMessage 再二次路由。
- 推荐保留 CommunicationHub、LegacySocketAdapter、MessageRouter，但把它们降级为“可选通信适配包”，只在 socket 模式或兼容模式下初始化。
- 推荐引入一个新的极薄接口 ILogicRuntimePort，替代 ILogicGateway，作为 UI 层到 logic 层的唯一最小边界。它只保留基础框架真正需要的两个能力：发送 UiAction 和请求 resync。
- 不推荐让 UiActionDispatcher 直接依赖具体 LogicRuntime 类。虽然这比 current gateway 更简单，但会把 UI 层直接绑定到 runtime 具体实现，削弱测试替身能力。用一个更薄的 ILogicRuntimePort 可以同时保留简单性和测试边界。
- 推荐的最小本地主链路如下：Page 通过 UiActionDispatcher 发送动作，UiActionDispatcher 调用 ILogicRuntimePort，LogicRuntime 根据 moduleId 或 targetModule 找到 ModuleLogicHandler，ModuleLogicHandler 更新 SceneGraph 或发出 LogicNotification，ApplicationCoordinator 和 ModuleCoordinator 再把通知返回到 page 或 shell。
- 推荐的迁移原则是：优先收缩主链路，不优先动模块页面；优先删除中间转发层，不优先重写业务逻辑；优先把通信层降级为插件，不优先重写通信协议。

**保留类**
- MainWindow — 继续作为应用顶层窗口和 overlay/tool host 宿主。
- WorkspaceShell — 继续作为 center/right/bottom/top 挂载框架。
- GlobalUiManager — 继续保留，但在基础框架中是可选宿主能力，而不是强制依赖。
- BaseSoftwareInitializer — 继续作为软件装配骨架。
- ModuleUiAssemblers — 继续作为模块 page 和辅助区工厂入口。
- ApplicationCoordinator — 升格为明确的 UI 管理中心。
- ModuleCoordinator — 继续负责模块 page、模块私有 dispatcher 和辅助区生命周期。
- PageManager — 继续负责中心页面栈管理。
- UiActionDispatcher — 保留，但替换依赖类型和职责边界。
- LogicRuntime — 保留，并承担直接模块路由职责。
- ModuleLogicRegistry — 保留，继续作为 moduleId 到 logic handler 的映射中心。
- ModuleLogicHandler — 保留，继续作为模块逻辑抽象。
- SceneGraph — 保留，继续作为场景真相层。
- LogicNotification — 保留，继续作为 logic 到 UI 的统一通知对象。

**删除类**
- ILogicGateway — 删除。它当前承担的连接状态、订阅管理、动作发送接口，对基础 UI 框架来说过重，且命名语义也不再准确。
- LocalLogicGateway — 删除。它的主价值在于本地与 socket 模式之间的双路径判断和重同步转发，不属于基础 UI 框架必要能力。
- AppMessageCenter — 删除。其 publish、dispatchUiIntent、dispatchStateSample、dispatchModuleEvent 这套通用消息总线能力超出了基础本地框架所需复杂度。

**降级为可选兼容层的类**
- CommunicationHub — 保留，但只在 socket 软件变体中初始化，不再参与纯本地 UI 主链路。
- MessageRouter — 保留在兼容包中，仅服务旧 envelope 分类、去重和外部消息路由。
- LegacySocketAdapter — 保留在兼容包中，仅服务旧协议 envelope 转换。

**替代关系**
- ILogicGateway -> ILogicRuntimePort。替代原因：去掉连接状态和订阅职责，只保留动作分发和 resync 能力。
- LocalLogicGateway -> 无直接替代类。替代方式：UiActionDispatcher 直接面向 ILogicRuntimePort，ApplicationCoordinator 直接连接 LogicRuntime 的 logicNotification 信号。
- AppMessageCenter -> LogicRuntime 内部直接路由。替代原因：UI intent 和 state sample 的主路径不需要再套一层通用消息模型。
- 本地 loopback 通道 -> 直接本地调用。替代原因：基础框架先保证本地调用闭环，外部通信模式再按需挂接。
- “通信为主、UI 回环”模式 -> “本地 UI 为主、通信适配为辅”模式。替代原因：符合当前目标，也更容易维护。

**类级迁移矩阵**
- UiActionDispatcher：构造参数从 ILogicGateway* 改为 ILogicRuntimePort*；sendAction 和 requestResync 直接调用最小 runtime port。
- ApplicationCoordinator：构造参数从 ILogicGateway* 改为 ILogicRuntimePort*；不再经 gateway 接收通知，而是直接连接 LogicRuntime::logicNotification。
- ModuleCoordinator：构造参数从 ILogicGateway* 改为 ILogicRuntimePort*；其余职责不变。
- BaseSoftwareInitializer：不再创建 LocalLogicGateway；改为创建或注入 ILogicRuntimePort 实例，并直接把 LogicRuntime 的通知连到 ApplicationCoordinator。
- LogicRuntime：内联当前 AppMessageCenter 对 UiAction 和 StateSample 的目标解析与 handler 路由逻辑。
- CommunicationHub：继续向 LogicRuntime 暴露 onControlMessageReceived、onServerCommandReceived、onStateSampleReceived 等兼容入口，但只在 socket 模式装配。

**实施细化：类/函数级改动清单**
- 第一阶段新增文件：d:/code/C++/VTKQtCore/src/logic/runtime/ILogicRuntimePort.h。
- ILogicRuntimePort 推荐只定义两个纯虚方法：sendAction(const UiAction& action) 与 requestResync(const QString& reason)。不引入连接状态、订阅、通知转发等职责。
- LogicRuntime.h：让 LogicRuntime 继承 ILogicRuntimePort；删除 AppMessageCenter 前置声明、getAppMessageCenter() 声明和 m_appMessageCenter 成员；新增 sendAction(const UiAction& action) override。
- LogicRuntime.cpp：构造函数删除 AppMessageCenter 创建、setModuleRegistry、setActiveModuleState 和相关 connect；sendAction() 直接转发到 onActionReceived()；routeToModuleHandler() 改为直接解析 targetModule 并调用 ModuleLogicRegistry::getHandler()/resolveModuleId()；onStateSampleReceived() 直接完成 sample 目标解析与 global 广播。
- UiActionDispatcher.h：前置声明由 ILogicGateway 改为 ILogicRuntimePort；构造函数参数改为 ILogicRuntimePort*；成员 m_gateway 改为 m_runtimePort。
- UiActionDispatcher.cpp：include 从 logic/gateway/ILogicGateway.h 改为 logic/runtime/ILogicRuntimePort.h；sendAction() 调用 m_runtimePort->sendAction(action)；requestResync() 调用 m_runtimePort->requestResync(reason)。
- ApplicationCoordinator.h：构造函数参数从 ILogicGateway* 改为 ILogicRuntimePort*；成员 m_gateway 改为 m_runtimePort。
- ApplicationCoordinator.cpp：构造 UiActionDispatcher 时传入 runtimePort；删除对 gateway 的任何语义依赖；如果后续无额外用途，可移除 m_runtimePort 成员，仅在构造阶段向 dispatcher 传入即可。
- ModuleCoordinator.h：构造参数从 ILogicGateway* 改为 ILogicRuntimePort*；成员 m_gateway 改为 m_runtimePort 或直接删除该成员，仅保留 dispatcher。
- ModuleCoordinator.cpp：构造 UiActionDispatcher 时传入 runtimePort；其余 activate()/deactivate()/onModuleNotification() 保持不动。
- ModuleUiAssemblers.h：ModuleUiAssemblyContext 中 gateway 字段改为 runtimePort 字段，类型为 ILogicRuntimePort*。
- ModuleUiAssemblers.cpp：所有 new ModuleCoordinator(moduleId, ...) 调用改为传 runtimePort；其余页面装配不动。
- BaseSoftwareInitializer.h：initialize() 第三个参数从 ILogicGateway* 改为 ILogicRuntimePort*；registerModuleUIs() 与 registerShellModules() 的 gateway 参数同步改名并改类型。
- BaseSoftwareInitializer.cpp：删除 include ILogicGateway；initialize() 中不再 connect(gateway, &ILogicGateway::notificationReceived, ...)，替换为 connect(logicRuntime, &LogicRuntime::logicNotification, m_appCoordinator, &ApplicationCoordinator::onShellNotification)；registerModuleUIs()/registerShellModules() 调用传 runtimePort。
- DefaultSoftwareInitializer.h：registerModuleUIs() 和 registerShellModules() override 签名同步替换为 ILogicRuntimePort*。
- DefaultSoftwareInitializer.cpp：gatewayStateName(ILogicGateway*) 替换为基于 run mode 与 CommunicationHub 当前状态的轻量状态计算；InterModuleSenderWidget 的 dispatcher 继续保留；InterModuleReceiverWidget 构造参数改为 LogicRuntime* 或更通用的 notification source；workflowMenu 的 notification 连接改为直接连接 LogicRuntime::logicNotification。
- main.cpp：删除 logic/gateway/LocalLogicGateway.h include 与 LocalLogicGateway gateway 实例；调用 initializer->initialize(&mainWindow, &logicRuntime, &logicRuntime, communicationHubPtr)；CommunicationHub 改为仅在 socket 模式下创建、initialize、setServerEndpoint、start/stop，并以指针形式传递给 initializer。
- ModuleUiEventBinding.h：bind() 不再依赖 ILogicGateway::notificationReceived，改为模板化的 notification source 绑定，直接连接到发出 logicNotification(const LogicNotification&) 的对象。
- InterModuleReceiverWidget.h/.cpp：构造参数从 ILogicGateway* 改为 LogicRuntime* 或 generic notification source；槽函数 onGatewayNotification() 建议重命名为 onLogicNotification() 或 onNotification()。
- ModuleNavigationModule.h/.cpp：槽函数 onGatewayNotification() 建议重命名为 onLogicNotification()；调用点从 DefaultSoftwareInitializer 里连接 logicRuntime->logicNotification。
- LogicRuntime.h/.cpp：保留 onControlMessageReceived()/onServerCommandReceived()/onModulePollBatch() 等通信入口，用于兼容层，但这些入口只属于 Optional Communication Adapter Layer，不再服务 UI 主链路。

**实施细化：AppMessageCenter 内联范围**
- routeUiAction() 中保留的核心逻辑只有三项：解析 targetModule、走 ModuleLogicRegistry::resolveModuleId()、查找 handler 并调用 handler->handleAction(action)。
- reject() 中的错误通知逻辑不建议原样整体搬迁；推荐仅保留必要的 shell 级错误通知生成，合并进 LogicRuntime 内部已有 createShellError() 路径，避免复制第二套错误模型。
- routeStateSample() 中保留的核心逻辑只有两项：global 广播与单模块 resolveModuleId 后调用 handler->handleStateSample()。
- publish()、dispatchModuleEvent()、routeShellMessage() 不进入基础框架主链路；若兼容模式后续仍需保留，建议移动到通信适配包或专用 adapter，而不是继续保留在 LogicRuntime 核心中。

**实施细化：额外受影响文件**
- d:/code/C++/VTKQtCore/src/ui/coordination/ModuleUiEventBinding.h — 当前直接绑定 ILogicGateway::notificationReceived，删除 gateway 后必须一起改。
- d:/code/C++/VTKQtCore/src/modules/intermoduletest/InterModuleReceiverWidget.h — 当前构造依赖 ILogicGateway*。
- d:/code/C++/VTKQtCore/src/modules/intermoduletest/InterModuleReceiverWidget.cpp — 当前通过 gateway 订阅 notificationReceived，并调用 ModuleUiEventBinding::bind()。
- d:/code/C++/VTKQtCore/src/modules/workflowshell/ModuleNavigationModule.h — 当前存在 onGatewayNotification(const LogicNotification&) 槽函数，命名和连接都需要同步调整。
- d:/code/C++/VTKQtCore/src/modules/workflowshell/ModuleNavigationModule.cpp — 当前消费 gateway 风格通知，后续应消费 logicNotification。
- d:/code/C++/VTKQtCore/src/app/software/ConcreteSoftwareInitializers/DefaultSoftwareInitializer.cpp — 当前既用 gatewayStateName() 又直接 connect(gateway, &ILogicGateway::notificationReceived, ...)，是 gateway 删除后的关键适配点。

**实施细化：分阶段交付边界**
- 阶段 1 只做边界替换，不删文件。新增 ILogicRuntimePort，并让 LogicRuntime 实现它；UiActionDispatcher/ApplicationCoordinator/ModuleCoordinator/BaseSoftwareInitializer/ModuleUiAssemblers 全部切到新接口，但 LocalLogicGateway、AppMessageCenter 文件暂不删除。
- 阶段 2 切通知源。把所有 notificationReceived 连接迁移到 LogicRuntime::logicNotification，包括 BaseSoftwareInitializer、DefaultSoftwareInitializer、InterModuleReceiverWidget、ModuleUiEventBinding 相关路径。
- 阶段 3 内联路由。把 AppMessageCenter 的 routeUiAction/routeStateSample 逻辑并入 LogicRuntime；此阶段完成后，AppMessageCenter 应只剩未再使用的残余接口。
- 阶段 4 删除中间层。删除 ILogicGateway、LocalLogicGateway、AppMessageCenter，并清理 include、前置声明、构造参数和文档引用。
- 阶段 5 降级通信层。main.cpp 和 BaseSoftwareInitializer 只在 socket 模式装配 CommunicationHub；本地模式下不再初始化任何通信适配对象。

**实施细化：推荐单一迁移顺序**
1. 新增 ILogicRuntimePort，并让 LogicRuntime 实现。
2. 改 UiActionDispatcher。
3. 改 ModuleCoordinator、ApplicationCoordinator、ModuleUiAssemblyContext。
4. 改 BaseSoftwareInitializer、DefaultSoftwareInitializer、main.cpp。
5. 改所有 notificationReceived 使用点。
6. 内联 AppMessageCenter 路由逻辑。
7. 删除 ILogicGateway、LocalLogicGateway、AppMessageCenter。
8. 最后把 CommunicationHub 降级为仅 socket 模式装配。

**构建层说明**
- 当前 CMakeLists.txt 使用 GLOB_RECURSE 自动收集 src/ 下全部 .h/.cpp/.ui/.qrc，因此删除 ILogicGateway、LocalLogicGateway、AppMessageCenter 文件后，不需要手动维护源文件列表。
- 但删除文件后仍需检查 include 链和前置声明是否已全部清理，否则虽然构建脚本无需调整，编译依赖仍会残留。

**实施阶段**
1. 收缩接口边界。新增 ILogicRuntimePort，定义 sendAction(const UiAction&) 与 requestResync(const QString&) 两个最小能力；同时为 LogicRuntime 提供该接口实现。
2. 改造 UI 侧依赖。把 UiActionDispatcher、ApplicationCoordinator、ModuleCoordinator 的构造依赖从 ILogicGateway 替换为 ILogicRuntimePort。
3. 改造启动装配。调整 BaseSoftwareInitializer 和 main.cpp 的装配流程，不再创建 LocalLogicGateway，本地模式下直接把 LogicRuntime 作为 UI 动作和通知的中心对象。
4. 迁移通知链路。将 ApplicationCoordinator 从“监听 gateway 的 notificationReceived”改为直接连接 LogicRuntime::logicNotification；移除 gateway 的订阅和转发逻辑。
5. 内联 action 路由。把 AppMessageCenter::dispatchUiIntent 的目标解析和 handler 查找逻辑并入 LogicRuntime::onActionReceived()/routeToModuleHandler()。
6. 内联 state sample 路由。把 AppMessageCenter::dispatchStateSample 的目标解析和 global 广播逻辑并入 LogicRuntime::onStateSampleReceived() 或其内部辅助方法。
7. 删除中间层代码。移除 ILogicGateway、LocalLogicGateway、AppMessageCenter 及其在构造、连接、include、文档中的引用。
8. 降级通信层。把 CommunicationHub、MessageRouter、LegacySocketAdapter 明确归为 Optional Communication Adapter Layer；在纯本地基础框架中不初始化、不参与 UI 主链路。
9. 回归验证。分别验证本地模式启动、模块切换、datagen 节点更新、params 参数更新、planning 和 navigation 的通知链路，以及 socket 模式下的兼容适配是否仍可单独接回 LogicRuntime。

**相关文件 / 模块**
- d:/code/C++/VTKQtCore/src/ui/coordination/ApplicationCoordinator.h — 当前全局 UI 协调中心，未来应正式承担 UI 管理中心角色。
- d:/code/C++/VTKQtCore/src/ui/coordination/ApplicationCoordinator.cpp — 当前页面切换和通知转发实现。
- d:/code/C++/VTKQtCore/src/ui/coordination/ModuleCoordinator.h — 当前模块级 page 与辅助区协调入口。
- d:/code/C++/VTKQtCore/src/ui/coordination/ModuleCoordinator.cpp — 当前模块激活、停用和通知转发实现。
- d:/code/C++/VTKQtCore/src/ui/coordination/UiActionDispatcher.h — 当前 UI 动作发送入口，需要替换依赖类型。
- d:/code/C++/VTKQtCore/src/ui/coordination/UiActionDispatcher.cpp — 当前 sendAction/sendCommand/requestModuleSwitch 实现。
- d:/code/C++/VTKQtCore/src/ui/pages/PageManager.h — 当前页面栈管理器。
- d:/code/C++/VTKQtCore/src/shell/WorkspaceShell.h — 当前基础壳层布局。
- d:/code/C++/VTKQtCore/src/app/software/BaseSoftwareInitializer.h — 当前软件装配骨架，需要调整依赖注入路径。
- d:/code/C++/VTKQtCore/src/app/software/BaseSoftwareInitializer.cpp — 当前创建 PageManager、ApplicationCoordinator、PromptAudioService、gateway 和通信连接的核心入口。
- d:/code/C++/VTKQtCore/src/app/software/ModuleUiAssemblers.cpp — 当前模块 UI 装配工厂。
- d:/code/C++/VTKQtCore/src/logic/runtime/LogicRuntime.h — 当前逻辑运行时入口，未来应直接承担动作/状态路由职责。
- d:/code/C++/VTKQtCore/src/logic/runtime/LogicRuntime.cpp — 当前 switchToModule、onActionReceived、onStateSampleReceived 以及 AppMessageCenter 接入点。
- d:/code/C++/VTKQtCore/src/logic/runtime/AppMessageCenter.h — 当前待删除的通用消息总线类。
- d:/code/C++/VTKQtCore/src/logic/gateway/ILogicGateway.h — 当前待删除的过重抽象接口。
- d:/code/C++/VTKQtCore/src/logic/gateway/LocalLogicGateway.h — 当前待删除的本地/远端双路径实现。
- d:/code/C++/VTKQtCore/src/logic/runtime/ILogicRuntimePort.h — 新增的最小 runtime 边界接口，建议作为 UI 到 logic 的唯一抽象入口。
- d:/code/C++/VTKQtCore/src/communication/hub/CommunicationHub.h — 当前待降级为可选通信适配层的入口。
- d:/code/C++/VTKQtCore/src/communication/routing/MessageRouter.h — 当前待降级为可选兼容路由器。
- d:/code/C++/VTKQtCore/src/communication/routing/LegacySocketAdapter.h — 当前待降级为可选旧协议适配器。
- d:/code/C++/VTKQtCore/src/logic/registry/ModuleLogicRegistry.h — 当前模块逻辑注册中心。
- d:/code/C++/VTKQtCore/src/logic/registry/ModuleLogicHandler.h — 当前模块逻辑抽象基类。
- d:/code/C++/VTKQtCore/src/logic/scene/SceneGraph.h — 当前场景真相层，保持不动。
- d:/code/C++/VTKQtCore/src/ui/coordination/ModuleUiEventBinding.h — 当前额外依赖 gateway 通知，需要一并改造。
- d:/code/C++/VTKQtCore/src/modules/intermoduletest/InterModuleReceiverWidget.h — 当前额外依赖 gateway 通知，需要一并改造。
- d:/code/C++/VTKQtCore/src/modules/workflowshell/ModuleNavigationModule.h — 当前存在 gateway 风格通知槽函数，需要同步调整命名与连接源。

**验收标准**
- 纯本地模式下，应用启动不再依赖 LocalLogicGateway、AppMessageCenter、CommunicationHub 的 loopback 才能完成 page 初始化和模块切换。
- ApplicationCoordinator 能明确作为 UI 管理中心工作：注册模块、切换 page、挂载辅助区、分发 LogicNotification。
- 模块 page 的写法基本保持不变，只需要继续持有 UiActionDispatcher，不需要理解通信层。
- 模块 logic 继续通过 ModuleLogicHandler 处理动作和状态样本，并通过 SceneGraph 和 LogicNotification 驱动显示。
- UI 动作主链路被压缩到 3 跳以内：Page -> UiActionDispatcher -> LogicRuntime -> ModuleLogicHandler。
- socket 兼容能力仍可保留为可选模式，但不再污染本地基础框架主路径。

**验证方式**
1. 启动验证：在纯本地模式下启动应用，确认无需 CommunicationHub 也可进入默认模块。
2. 模块切换验证：通过 shell 或页面触发 requestModuleSwitch，确认 ApplicationCoordinator 正常切页和挂载辅助区。
3. datagen 验证：触发 create_node，确认节点更新、SceneGraph 更新和 UI 刷新链路仍成立。
4. params 验证：触发 apply_parameters，确认页面状态刷新仍正常。
5. planning/navigation 验证：确认 SceneNodesUpdated、StageChanged、CustomEvent 等通知仍能路由到页面。
6. 兼容模式验证：在启用 socket 适配层的模式下，确认 CommunicationHub 仍能把外部消息送达 LogicRuntime。

**风险与缓解**
- 风险：删除 LocalLogicGateway 后，当前通过 gateway 转发通知的链路会断。 — 缓解：把 ApplicationCoordinator 直接连接到 LogicRuntime::logicNotification，并在阶段 4 先完成通知链路迁移后再删 gateway。
- 风险：删除 AppMessageCenter 后，targetModule 解析和 alias 解析可能丢失。 — 缓解：把 resolveTargetModule 相关能力迁移到 LogicRuntime 内部，并继续复用 ModuleLogicRegistry 的解析能力。
- 风险：通信层降级为可选后，socket 兼容模式可能在某些初始化路径下失联。 — 缓解：在 BaseSoftwareInitializer 中把通信装配显式绑定到 run mode，不再隐式依赖本地主链路回环。
- 风险：直接让 UiActionDispatcher 依赖 LogicRuntime 会降低测试替身能力。 — 缓解：采用 ILogicRuntimePort 而不是直接绑定 LogicRuntime 具体类。
- 风险：一次性同时删除 gateway 和 message center 会放大回归面。 — 缓解：按阶段迁移，先替换边界接口，再内联路由，最后再删中间层代码。

**决策与假设**
- 已确认：ApplicationCoordinator 作为 UI 管理中心是可行且优于新建并行中心类的方案。
- 已确认：基础框架主链路不应再经过 LocalLogicGateway 和 CommunicationHub loopback。
- 已确认：AppMessageCenter 对当前目标而言属于过度通用设计，基础框架可删除。
- 已确认：CommunicationHub、MessageRouter、LegacySocketAdapter 不是必须删除，而是要从主链路降级为可选兼容包。
- 已确认：第一阶段建议完整保留 CommunicationHub 兼容能力，只把它从本地主链路中剥离，不在第一步同时动协议适配实现。
- 当前假设：ModuleLogicHandler、SceneGraph、LogicNotification 的模块逻辑模式继续保留，不另起新的 UI-only 状态管理体系。
- 当前假设：模块 page、VTK 窗口和辅助区的装配方式继续沿用 BaseSoftwareInitializer + ModuleUiAssemblers，不额外引入 declarative UI schema。

**待确认项**
- 无阻塞项。当前文档已从“实施前方案”更新为“已落地基线 + 后续验证清单”。