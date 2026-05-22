# datagen 模块规格

## 基本信息

| 属性 | 值 |
| --- | --- |
| 模块 ID | datagen |
| Handler 类 | DataGenModuleLogicHandler |
| Page 类 | DataGenPage |
| UI 装配入口 | registerDataGenModuleUi |
| 主要通知 | SceneNodesUpdated, TipChanged |
| 主要场景对象 | PointNode, LineNode, ModelNode, PlaneNode, TransformNode |

## 职责与边界

### 核心职责

- 创建、修改、删除 SceneGraph 节点
- 维护几何属性、显示属性和父子变换关系
- 作为场景图生成和调试入口
- 在页面中展示当前节点概况和 3D 结果

### 不负责

- 规划算法
- 导航状态机
- 参数持久化

## 输入契约

### 典型 UiAction 命令

- create_node
- delete_node
- update_display
- assign_parent
- update_transform_pose
- add_point
- add_line_vertex
- update_plane_geometry
- clear_node_geometry
- seed_demo
- test_prompt_play_burst

### 输入行为说明

| 命令 | 作用 |
| --- | --- |
| create_node | 创建指定类型节点 |
| delete_node | 删除指定节点并清理引用 |
| update_display | 修改颜色、透明度、宽度等显示属性 |
| assign_parent | 设置父变换关系 |
| update_transform_pose | 更新 TransformNode 位姿 |
| add_point | 向点集追加点 |
| add_line_vertex | 向线节点追加顶点 |
| seed_demo | 创建演示结构 |

## 输出契约

### 主要 LogicNotification

| 类型 | 目标 | 关键字段 |
| --- | --- | --- |
| SceneNodesUpdated | AllModules | nodeSummaries, transformOptions, statusText |
| TipChanged | Shell | 状态提示文本 |

## SceneGraph 影响

datagen 是当前最强的 SceneGraph 写入模块。

### 会创建或维护的节点类型

- PointNode
- LineNode
- ModelNode
- PlaneNode
- TransformNode

### 典型写入模式

1. create_node 时创建节点并 addNode()
2. update_display 时同步材质、颜色、可见性、图层等属性
3. assign_parent 时修改 parentTransform 引用
4. delete_node 时 removeNode() 并清理父子关系

## 页面与辅助区

### 主页面

- DataGenPage
- 注入模块私有 UiActionDispatcher
- 挂一个名为 datagen_main 的 VtkSceneWindow

### 辅助区

- 右侧摘要面板
- 显示 nodeSummaries 数量与状态文本

## 典型工作流

### 创建节点

1. 用户在 DataGenPage 选择节点类型并填写参数。
2. 页面通过 UiActionDispatcher 发送 create_node。
3. DataGenModuleLogicHandler 创建节点并写入 SceneGraph。
4. 模块发出 SceneNodesUpdated。
5. DataGenPage 和摘要面板同时刷新。
6. datagen_main 触发 requestReconcile() 或下一轮渲染更新。

### 删除节点

1. 用户选择节点并触发 delete_node。
2. handler 从 SceneGraph 删除节点。
3. 删除时清理父引用和选择状态。
4. 广播 SceneNodesUpdated。

## 最小验证路径

1. 启动应用，进入 datagen。
2. 创建一个 PointNode。
3. 检查节点列表、摘要面板和 3D 窗口是否同时更新。
4. 修改显示属性，确认颜色或可见性变化。
5. 删除节点，确认 3D 场景和列表同步清除。

## 源码索引

- [../../src/modules/datagen/DataGenModuleLogicHandler.h](../../src/modules/datagen/DataGenModuleLogicHandler.h)
- [../../src/modules/datagen/DataGenModuleLogicHandler.cpp](../../src/modules/datagen/DataGenModuleLogicHandler.cpp)
- [../../src/modules/datagen/DataGenPage.h](../../src/modules/datagen/DataGenPage.h)
- [../../src/modules/datagen/DataGenPage.cpp](../../src/modules/datagen/DataGenPage.cpp)
- [../../src/app/software/ModuleUiAssemblers.cpp](../../src/app/software/ModuleUiAssemblers.cpp)
