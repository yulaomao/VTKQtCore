# planning 模块规格

## 基本信息

| 属性 | 值 |
| --- | --- |
| 模块 ID | planning |
| Handler 类 | PlanningModuleLogicHandler |
| Page 类 | PlanningPage |
| UI 命令头 | PlanningUiCommands.h |
| 主要通知 | SceneNodesUpdated, StageChanged, CustomEvent |
| VTK 窗口 | planning_main, planning_overview |

## 职责与边界

### 核心职责

- 生成规划路径
- 维护规划状态机
- 把规划结果映射为路径线段、箭头和模型可视化
- 同时驱动主视图和概览视图

### 不负责

- 参数表单维护
- 导航实时跟踪

## 输入契约

### 典型 UiAction 命令

- generate_plan
- accept_plan

### StateSample

- pathPoints 数组
- 规划状态恢复数据

## 输出契约

| 类型 | 目标 | 关键字段 |
| --- | --- | --- |
| SceneNodesUpdated | AllModules | status, pathVisualization, pathPoints |
| StageChanged | 当前模块或广播 | status |
| CustomEvent | ModuleList 或其他目标范围 | 规划状态变化事件 |

## SceneGraph 影响

planning 会维护多类规划可视化节点：

- 规划模型节点
- 路径线段节点数组
- 导向箭头节点

### 关键行为

- ensurePlanPathSegmentNodes() 根据路径点数量动态增删线段节点
- syncPlanPathVisualization() 把 pathPoints 同步到显示节点
- removeLegacyPlanPathNodes() 清理旧规划结果

## 页面与辅助区

### 主页面

- PlanningPage
- 主场景窗口 planning_main
- 概览窗口 planning_overview
- 规划状态标签
- 生成、接受、测试相关按钮

### 辅助区

- 右侧摘要面板
- 用于显示当前规划状态

## 典型工作流

1. 用户点击 generate_plan。
2. handler 生成或接收 pathPoints。
3. handler 创建/更新路径线段和箭头节点。
4. 发出 SceneNodesUpdated 或 StageChanged。
5. PlanningPage 更新状态文本。
6. 两个 VTK 窗口在激活或渲染周期中同步刷新。

## 最小验证路径

1. 进入 planning。
2. 触发 generate_plan。
3. 检查 planning_main 和 planning_overview 是否都出现路径结果。
4. 检查右侧摘要和页面状态是否更新。
5. 接受或重置规划后，确认旧路径节点被正确清理或更新。

## 源码索引

- [../../src/modules/planning/PlanningModuleLogicHandler.h](../../src/modules/planning/PlanningModuleLogicHandler.h)
- [../../src/modules/planning/PlanningModuleLogicHandler.cpp](../../src/modules/planning/PlanningModuleLogicHandler.cpp)
- [../../src/modules/planning/PlanningPage.h](../../src/modules/planning/PlanningPage.h)
- [../../src/modules/planning/PlanningPage.cpp](../../src/modules/planning/PlanningPage.cpp)
- [../../src/modules/planning/PlanningUiCommands.h](../../src/modules/planning/PlanningUiCommands.h)
- [../../src/app/software/ModuleUiAssemblers.cpp](../../src/app/software/ModuleUiAssemblers.cpp)
