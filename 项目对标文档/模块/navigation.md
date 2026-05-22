# navigation 模块规格

## 基本信息

| 属性 | 值 |
| --- | --- |
| 模块 ID | navigation |
| Handler 类 | NavigationModuleLogicHandler |
| Page 类 | NavigationPage |
| UI 命令头 | NavigationUiCommands.h |
| 主要通知 | StageChanged, SceneNodesUpdated, CustomEvent |
| VTK 窗口 | navigation_main |

## 职责与边界

### 核心职责

- 跟踪远程变换数据
- 维护导航启停状态和当前位置
- 把远程变换映射为本地 TransformNode
- 输出变换健康度相关通知

### 不负责

- 规划路径生成
- 参数维护

## 输入契约

### 典型 UiAction 命令

- start_navigation
- stop_navigation

### StateSample

- remoteTransformId
- x, y, z
- rotMatrix 或等价姿态数据

## 输出契约

| 类型 | 目标 | 关键字段 |
| --- | --- | --- |
| StageChanged | 当前模块 | status, navigating |
| SceneNodesUpdated | AllModules 或相关目标 | currentPosition |
| CustomEvent | Shell 或其他目标 | transform_health_check 等健康事件 |

## SceneGraph 影响

navigation 主要维护 TransformNode 映射，而不是复杂几何。

### 核心状态

- m_navigating
- m_navigationStatus
- m_currentPositionX/Y/Z
- m_localTransformNodeIdsByRemoteId
- m_lastSampleTimestampMsByRemoteId

### 关键行为

- ensureTrackedTransformNode() 保证每个 remoteTransformId 对应本地节点
- applyTransformSample() 写入姿态
- emitTransformHealth() 输出健康状态相关事件

## 页面与辅助区

### 主页面

- NavigationPage
- 状态标签
- 开始/停止按钮
- 当前位置显示
- 单个 VTK 窗口 navigation_main

### 辅助区

- 右侧摘要面板
- 显示当前导航状态文本

## 典型工作流

1. 用户点击 start_navigation。
2. handler 设置 m_navigating=true。
3. 外部状态样本持续输入姿态数据。
4. handler 更新本地 TransformNode 并刷新当前位置。
5. 页面状态、位置显示和 3D 窗口同步变化。
6. 若数据长时间未更新，则通过健康事件提示异常。

## 最小验证路径

1. 进入 navigation。
2. 点击开始导航。
3. 注入一组远程位姿样本。
4. 检查页面状态、位置显示和 3D 场景是否同步更新。
5. 停止导航后，确认 navigating 标志和状态文本切换正确。

## 源码索引

- [../../src/modules/navigation/NavigationModuleLogicHandler.h](../../src/modules/navigation/NavigationModuleLogicHandler.h)
- [../../src/modules/navigation/NavigationModuleLogicHandler.cpp](../../src/modules/navigation/NavigationModuleLogicHandler.cpp)
- [../../src/modules/navigation/NavigationPage.h](../../src/modules/navigation/NavigationPage.h)
- [../../src/modules/navigation/NavigationPage.cpp](../../src/modules/navigation/NavigationPage.cpp)
- [../../src/modules/navigation/NavigationUiCommands.h](../../src/modules/navigation/NavigationUiCommands.h)
- [../../src/app/software/ModuleUiAssemblers.cpp](../../src/app/software/ModuleUiAssemblers.cpp)
