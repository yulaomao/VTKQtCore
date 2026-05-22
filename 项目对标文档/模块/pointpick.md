# pointpick 模块规格

## 基本信息

| 属性 | 值 |
| --- | --- |
| 模块 ID | pointpick |
| Handler 类 | PointPickModuleLogicHandler |
| Page 类 | PointPickPage |
| 辅助面板 | PointPickStatusPanel |
| UI 命令头 | PointPickUiCommands.h |
| 主要通知 | SceneNodesUpdated |

## 职责与边界

### 核心职责

- 从已有点集或外部状态中维护选中点列表
- 维护确认状态 confirmed
- 向页面和状态面板同步点数量、点列表、确认状态

### 不负责

- 生成新几何体
- 规划路径
- 导航实时位姿

## 输入契约

### 典型 UiAction 命令

- confirm_points

### StateSample

- 点数据
- 选中索引
- 确认状态

## 输出契约

| 类型 | 目标 | 关键字段 |
| --- | --- | --- |
| SceneNodesUpdated | AllModules | points, pointCount, selectedIndices, confirmed |

## SceneGraph 影响

pointpick 通常维护一个选择容器节点，而不是创建复杂的新几何对象。

### 主要行为

- onModuleActivated() 时确保选择容器节点存在
- 同步外部点数据到容器节点或内部选择状态
- 确认动作后广播选择状态

## 页面与辅助区

### 主页面

- PointPickPage
- 点列表
- 点计数
- 确认按钮

### 辅助区

- PointPickStatusPanel
- 显示点数量和确认状态

## 典型工作流

1. 外部状态样本或内部选择变化到来。
2. handler 更新点列表和确认状态。
3. 发出 SceneNodesUpdated。
4. PointPickPage 更新点列表、点计数和确认状态。
5. PointPickStatusPanel 同步相同状态。

## 最小验证路径

1. 进入 pointpick。
2. 输入或模拟一组点数据。
3. 检查页面是否展示点列表和点数。
4. 点击确认。
5. 检查 confirmed 状态是否同步到页面和状态面板。

## 源码索引

- [../../src/modules/pointpick/PointPickModuleLogicHandler.h](../../src/modules/pointpick/PointPickModuleLogicHandler.h)
- [../../src/modules/pointpick/PointPickModuleLogicHandler.cpp](../../src/modules/pointpick/PointPickModuleLogicHandler.cpp)
- [../../src/modules/pointpick/PointPickPage.h](../../src/modules/pointpick/PointPickPage.h)
- [../../src/modules/pointpick/PointPickPage.cpp](../../src/modules/pointpick/PointPickPage.cpp)
- [../../src/modules/pointpick/PointPickStatusPanel.h](../../src/modules/pointpick/PointPickStatusPanel.h)
- [../../src/modules/pointpick/PointPickStatusPanel.cpp](../../src/modules/pointpick/PointPickStatusPanel.cpp)
- [../../src/modules/pointpick/PointPickUiCommands.h](../../src/modules/pointpick/PointPickUiCommands.h)
