# 方案设计：UI去壳层化

本次要解决的问题是：当前基础框架把 UI 结构固化为 WorkspaceShell + center/right/bottom/top 分区，导致产品级界面骨架被框架预设，模块和具体软件初始化器只能在既定壳层里填内容，无法真正主导整体布局。推荐方案是彻底取消框架层对页面宿主、壳层分区和模块切换入口的预设，把“界面骨架定义权”下放给具体 SoftwareInitializer，同时在框架层只保留动作分发、运行时通信、模块注册与生命周期，以及一个用户可定义的全局控件工厂注册表。这样既满足你要求的“框架不再限制 UI”，又保留跨模块可复用、可全局访问的基础能力。

**需求摘要**
- 已确认目标：彻底重构现有 UI 框架，不保留旧版壳层。
- 已确认目标：框架层不再预设 top、right、bottom、center 等布局概念，也不再预设默认页面宿主。
- 已确认目标：总体布局采用混合模式，由具体 SoftwareInitializer 定义产品根界面骨架，模块再向该骨架贡献页面、面板或全局控件。
- 已确认目标：全局类控件仍然存在，但不再由框架预置固定实例，而是由用户定义控件类，通过工厂按需创建，并在全局范围可获取、可调用。
- 已确认目标：框架层继续保留动作分发与运行时通信能力，以及模块注册与生命周期能力。
- 已确认约束：框架层不再保留通用模块切换入口或默认导航 UI；切换逻辑由具体产品初始化器或产品层自行决定。
- 已确认约束：可以接受较大改造，不要求兼容旧版 WorkspaceShell 架构。
- 明确非目标：本次不保留旧的 WorkspaceShell/top-right-bottom 概念作为过渡层。
- 明确非目标：本次不在框架层继续维护固定全局控件集合。

**调研结论**
- 当前 UI 被绑死的核心不在 Page 内部，而在启动装配链上：BaseSoftwareInitializer 会把 PageManager 直接绑定到 WorkspaceShell 的 centerStack，ApplicationCoordinator 会把模块辅助区硬绑定到 right/bottom，DefaultSoftwareInitializer 会把导航和状态栏硬挂到 shell host。
- 当前源码中真正决定壳层结构的是 src/shell/WorkspaceShell.h 和 src/shell/WorkspaceShell.cpp；它把 top、center、right、bottom 与 shell host 做成了公开稳定概念。
- 当前模块本身已经具备较强的内部布局控制权：页面 QWidget 内部布局、VTK 窗口组织、模块动作发送、模块间 UI 事件都可以由模块自己决定；被框架抢走的是“顶层界面骨架”和“辅助区挂载语义”。
- 此前保留 WorkspaceShell、PageManager 和固定宿主骨架的旧方案与本次“去壳层化”方向冲突，因此不再继续保留。
- 当前规范文档 [项目对标文档/06-UI与Shell页面交互规范.md](项目对标文档/06-UI与Shell页面交互规范.md) 和 [项目对标文档/01-启动与装配规范.md](项目对标文档/01-启动与装配规范.md) 把 WorkspaceShell 与固定布局区域视为冻结项；按照本次约束，这部分后续应删除而不是保留。

**推荐方案**
- 推荐方向：把现有“框架负责壳层布局 + 初始化器往壳层填内容”的模型，改成“框架负责能力与契约，初始化器负责产品根界面骨架，模块负责自己的视图与可选贡献物”的模型。
- 框架层仅保留四类稳定职责：
- 第一，动作分发与运行时通信边界。继续保留 UiActionDispatcher 与 ILogicRuntimePort 这一类最小通信边界，页面和全局控件只通过该边界与 LogicRuntime 交互。
- 第二，模块注册与生命周期。框架仍负责模块定义注册、模块实例创建、激活/停用生命周期回调，以及运行时级别的 moduleId 解析与消息投递。
- 第三，全局控件工厂注册表。新增一个显式的 GlobalWidgetRegistry 或等价能力注册中心，但它只登记“工厂/描述”，不预创建控件实例；任何模块或初始化器在需要时按能力名或控件 ID 获取并创建控件。
- 第四，软件装配骨架。BaseSoftwareInitializer 不再负责创建标准壳层，而是只定义装配流程和扩展点，要求具体初始化器提供产品根 QWidget 树、全局控件工厂登记以及模块挂载策略。
- 不再保留 PageManager 作为“默认中心页面栈管理器”的框架强依赖。是否需要页栈、分栏、标签页、浮动布局或多窗口，由具体初始化器决定；如果某个产品仍需要 PageManager，可作为可选 helper，由初始化器主动选择而非框架强制注入。
- 不再保留 ApplicationCoordinator 作为“默认全局页面切换与辅助区挂载中枢”的框架固定角色。它要么被拆成更小的可选能力，要么降级为产品层可选协调器，而不是基础框架永远存在的中心类。
- 不再保留 ModuleCoordinator 中 Right/Bottom 这类区域语义。模块只暴露自身主视图、可选扩展视图、生命周期钩子和可选的全局控件贡献声明；这些视图最终如何排布，由产品初始化器决定。
- MainWindow 可以保留为应用根窗口，但它不再内含固定 WorkspaceShell。它最多只保留根容器、应用级 overlay 宿主、应用级 tool host 以及初始化器提供的产品根 widget 装配入口。
- GlobalUiManager 不能再绑定“通知条、遮罩、VTK 注册 + 固定全局挂载层”这一整套既定 UI 形态。建议把它拆成两层：一层是纯能力服务，例如 OverlayService、NotificationService、VtkWindowRegistry；另一层是可选的 UI 实现，由具体产品决定是否提供和如何挂载。
- 全局控件采用“工厂按需创建”而不是“全局单例实例”。原因是：你已经明确不希望框架预置全局控件；同时不同产品可能选择不同挂载位置、不同皮肤或不同生命周期。工厂模式比预创建单例更符合去壳层化目标。
- 模块切换不再是框架内建能力。若某个产品仍需要模块导航，应由该产品的 SoftwareInitializer 自己定义导航控件、当前模块状态存放位置和切换行为；框架只提供模块生命周期调用能力，不提供统一入口函数或统一导航 UI。
- 关键取舍：本次不走“保留旧壳层但设为可选”的温和改造路线，而是直接去掉旧壳层前提。原因是你已经明确要求彻底重构且不保留旧版壳层；保留过渡壳层只会把旧概念继续固化在公共接口里，后面更难删除。

**实施阶段**
1. 收缩框架职责。重新定义 BaseSoftwareInitializer 的职责边界，移除其中对 PageManager、WorkspaceShell、GlobalUiManager 固定实例和默认模块切换路径的依赖，明确它只负责装配流程、模块注册、运行时接线和产品根界面构建入口。
2. 定义新的产品根界面契约。新增一个由具体 SoftwareInitializer 实现的根界面提供接口，要求初始化器返回产品根 QWidget 树，并负责把模块主视图、产品级导航、产品级全局控件挂载到该树中。
3. 定义全局控件工厂注册表。新增全局控件/全局 UI 能力注册契约，支持按控件 ID、能力名或描述符登记工厂；调用方只能请求创建或查询工厂，不直接依赖某个预存在的固定控件实例。
4. 拆解旧的宿主型协调器。把 ApplicationCoordinator、ModuleCoordinator、PageManager、GlobalUiManager 中仍然必要的非壳层能力提取出来，分别归入模块生命周期、通知转发、VTK 窗口登记、可选产品协调器等边界；删除其中对固定区域和固定页面宿主的假设。
5. 去除固定区域语义。删除 WorkspaceShell 及其相关 topWidget、centerStack、rightWidget、bottomWidget、rightShellHost、bottomShellHost、mountRightAuxiliary、mountBottomAuxiliary 等公开概念；同步删除 ModuleCoordinator::AuxiliaryRegion::Right/Bottom 及相关使用点。
6. 重建模块装配模型。修改 ModuleUiAssemblers，使其不再把模块页面注册到 PageManager，不再默认创建右侧或底部辅助区，而是输出更中性的模块装配结果，例如主视图、可选扩展视图、可选全局控件贡献和模块生命周期绑定。
7. 重写具体初始化器。以 DefaultSoftwareInitializer 为样板，改成显式创建产品根 QWidget 树、决定如何挂载模块视图和全局控件、决定是否存在导航区域及其行为；旧的 registerShellModules 与固定壳层装配逻辑删除或重命名为产品装配逻辑。
8. 清理启动链路。修改 main.cpp 和启动装配文档，使启动流程不再出现“创建 WorkspaceShell、绑定 centerStack、通过标准 requestModuleSwitch(initialModule) 进入初始模块”的前提，而改为“创建 MainWindow、创建 initializer、由 initializer 装配产品根界面与初始显示状态”。
9. 更新规范与文档。删除或重写所有把 WorkspaceShell 和固定壳层区域视为稳定事实的文档，包括 [项目对标文档/01-启动与装配规范.md](项目对标文档/01-启动与装配规范.md) 与 [项目对标文档/06-UI与Shell页面交互规范.md](项目对标文档/06-UI与Shell页面交互规范.md)，并删除此前保留固定壳层宿主骨架的旧方案文档。
10. 分批迁移模块。优先让 datagen、params、planning、navigation 等模块先适配新的“中性装配结果”接口，再按产品初始化器定义的新根界面骨架挂载；完成后再删除残留的旧壳层兼容代码。

**相关文件 / 模块**
- d:/code/C++/VTKQtCore/src/app/software/BaseSoftwareInitializer.h — 当前装配骨架，需改成不依赖固定壳层的产品装配入口。
- d:/code/C++/VTKQtCore/src/app/software/BaseSoftwareInitializer.cpp — 当前直接创建 PageManager、GlobalUiManager、ApplicationCoordinator 并绑定 WorkspaceShell，需要按新职责拆解。
- d:/code/C++/VTKQtCore/src/app/software/ConcreteSoftwareInitializers/DefaultSoftwareInitializer.cpp — 当前以固定 shell host 装配导航、状态栏和模块 UI，后续应改为产品根界面装配样板。
- d:/code/C++/VTKQtCore/src/app/software/ModuleUiAssemblers.h — 当前上下文仍隐含 pageManager/globalUiManager/applicationCoordinator 等旧宿主依赖，需要改成中性装配输出契约。
- d:/code/C++/VTKQtCore/src/app/software/ModuleUiAssemblers.cpp — 当前按中心页面 + 右侧辅助区方式装配模块，需要改为不依赖固定区域的装配模型。
- d:/code/C++/VTKQtCore/src/shell/MainWindow.h — 当前主窗口仍持有 shell 与 overlay/tool host，需要收缩为更中性的根窗口职责。
- d:/code/C++/VTKQtCore/src/shell/MainWindow.cpp — 当前 rootStack 和 WorkspaceShell 装配逻辑需要调整为接纳 initializer 提供的根 widget。
- d:/code/C++/VTKQtCore/src/shell/WorkspaceShell.h — 当前固定区域接口定义，推荐删除。
- d:/code/C++/VTKQtCore/src/shell/WorkspaceShell.cpp — 当前固定区域实现，推荐删除。
- d:/code/C++/VTKQtCore/src/ui/coordination/ApplicationCoordinator.h — 当前全局页面切换与壳层通知中枢，需要拆分或降级为产品层可选对象。
- d:/code/C++/VTKQtCore/src/ui/coordination/ApplicationCoordinator.cpp — 当前 setCurrentModule、辅助区挂载和 shell 通知路由逻辑需要重写。
- d:/code/C++/VTKQtCore/src/ui/coordination/ModuleCoordinator.h — 当前暴露 Right/Bottom 辅助区语义，需要改成不含固定布局概念的模块协调契约。
- d:/code/C++/VTKQtCore/src/ui/coordination/ModuleCoordinator.cpp — 当前 activate/deactivate 附带固定辅助区行为，需要改为纯生命周期/通知转发职责。
- d:/code/C++/VTKQtCore/src/ui/pages/PageManager.h — 当前中心页面栈管理器，应从框架必选件降级为可选 helper 或被删除。
- d:/code/C++/VTKQtCore/src/ui/pages/PageManager.cpp — 当前中心页注册与切页逻辑，需要从默认启动链路移除。
- d:/code/C++/VTKQtCore/src/ui/globalui/GlobalUiManager.h — 当前混合了 UI 形态与服务能力，需要拆成服务层与可选 UI 实现。
- d:/code/C++/VTKQtCore/src/ui/globalui/AppStyleManager.h — 当前样式服务仍可保留，但应避免和固定壳层概念绑定。
- d:/code/C++/VTKQtCore/src/logic/runtime/ILogicRuntimePort.h — 保留为 UI 到运行时的最小稳定边界。
- d:/code/C++/VTKQtCore/src/logic/runtime/LogicRuntime.h — 继续作为运行时中心，承接动作和消息，但不再默认依赖固定 UI 壳层。
- d:/code/C++/VTKQtCore/src/main.cpp — 当前启动顺序需要改写为新装配模型。
- d:/code/C++/VTKQtCore/项目对标文档/01-启动与装配规范.md — 当前把 BaseSoftwareInitializer 和 WorkspaceShell 作为稳定事实，后续需删除旧冻结项并重写启动规范。
- d:/code/C++/VTKQtCore/项目对标文档/06-UI与Shell页面交互规范.md — 当前整章以 WorkspaceShell 固定区域为前提，后续需删除并重建。

**验收标准**
- 启动主链路中不再存在对 WorkspaceShell、centerStack、rightShellHost、bottomShellHost 等固定壳层概念的依赖。
- BaseSoftwareInitializer 不再在框架层创建默认页面宿主、默认壳层或默认模块切换路径。
- 任一具体 SoftwareInitializer 都能独立定义产品根 QWidget 树，并决定模块视图、导航控件、全局控件的挂载方式。
- 框架层仍能提供稳定的动作分发与运行时通信边界，页面和全局控件仍通过统一边界与 LogicRuntime 交互。
- 框架层仍能提供模块注册、模块实例管理和生命周期调用能力，但不提供统一的默认导航或默认页面切换 UI。
- 全局控件不再以固定实例预置在框架中，而是能够由用户定义控件类并通过工厂按需创建、全局访问。
- 现有模块迁移到新装配模型后，模块内部页面布局无需受 top/right/bottom 等区域语义限制。

**验证方式**
1. 启动验证：在移除 WorkspaceShell 前提后启动应用，确认 MainWindow 能正常显示由具体 initializer 构建的产品根界面。
2. 装配验证：为至少一个具体 initializer 创建非 center/right/bottom 结构的根界面，确认模块页面和产品级控件能够按初始化器定义成功挂载。
3. 运行时验证：在新结构下触发页面动作，确认 UiActionDispatcher 到 LogicRuntime 的主链路仍然有效。
4. 生命周期验证：激活、停用、销毁模块时，确认模块注册与生命周期钩子仍可正常工作，且不依赖默认页面栈。
5. 全局控件验证：通过全局控件工厂注册表按需创建一个全局控件，确认初始化器和模块都能获取并使用它。
6. 文档验证：确认启动规范和 UI 交互规范中已不再出现 WorkspaceShell 固定区域、默认中心页栈和默认辅助区的冻结定义。

**风险与缓解**
- 风险：一次性移除 WorkspaceShell、PageManager、ApplicationCoordinator 的默认角色，回归面会明显扩大。 — 缓解策略：先完成新契约抽取和新初始化器样板，再分批迁移模块，最后删除旧类。
- 风险：去掉框架内建模块切换后，各产品可能自行实现出多套不一致的导航逻辑。 — 缓解策略：框架不提供默认 UI，但可以提供可复用的产品层 helper 或推荐样板，不把导航 UI 固化进公共基类。
- 风险：GlobalUiManager 拆分后，通知、遮罩、VTK 注册等能力可能失去统一入口。 — 缓解策略：把服务能力与 UI 呈现拆开，统一保留服务接口，不统一保留呈现外壳。
- 风险：模块装配输出从“页面 + 右侧面板”改为中性结果后，改造量较大。 — 缓解策略：先定义稳定的中性装配契约，再按模块逐个迁移，避免边迁移边改契约。
- 风险：旧文档仍把固定壳层视为冻结事实，会误导后续实现。 — 缓解策略：把本方案作为唯一新推荐路径，并在后续规范更新中明确删除旧冻结项。

**决策与假设**
- 已确认：本次不是对旧壳层做可选化，而是直接取消旧壳层前提。
- 已确认：框架层仅保留动作分发与运行时通信、模块注册与生命周期，不再保留默认页面宿主与默认模块切换入口。
- 已确认：总体布局由具体 SoftwareInitializer 提供的产品根 QWidget 树决定。
- 已确认：全局控件通过工厂按需创建，而不是由框架预创建固定实例。
- 已确认：现有把 WorkspaceShell 固定区域当作冻结项的规范后续需要删除。
- 当前假设：MainWindow 仍可保留为应用根窗口，但其职责会显著收缩，不再是固定壳层宿主。
- 当前假设：ILogicRuntimePort 和 LogicRuntime 主链路继续保留，不另起新的 UI 到逻辑通信模型。
- 当前假设：VTK 窗口注册仍需要一个全局服务层，但该服务层不要求绑定某个固定 UI 壳层。

**待确认项**
- 无。当前边界已经足够形成单一推荐方案，并可直接交给实现型代理按阶段落地。