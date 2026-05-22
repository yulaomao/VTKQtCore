# SceneGraph 与显示规范

## 目标

本章定义 SceneGraph 作为场景真相层的角色、核心节点类型、节点修改与删除规则，以及显示层如何把 SceneGraph 同步到 VTK 渲染结果。

## SceneGraph 的角色定位

SceneGraph 是场景数据唯一真相，负责：

- 管理全部节点生命周期
- 维护节点 ID 到节点对象的映射
- 维护 TransformNode 树形层级
- 对节点事件提供统一广播
- 在批量修改时压缩渲染噪声

当前实现中，SceneGraph 内部以 QMap<QString, NodeBase*> 管理节点，并通过读写锁保证并发安全。

## 核心节点类型总表

| 节点类型 | 主要职责 | 典型显示管理器 |
| --- | --- | --- |
| TransformNode | 维护 4x4 变换矩阵和坐标系层级 | TransformNodeDisplayManager |
| PointNode | 存储点集、点颜色、尺寸、标签 | PointNodeDisplayManager |
| LineNode | 存储折线顶点、线宽、闭合状态、虚线状态 | LineNodeDisplayManager |
| ModelNode | 存储 vtkPolyData 与材质属性 | ModelNodeDisplayManager |
| PlaneNode | 存储平面几何参数与边框/填充状态 | PlaneNodeDisplayManager |
| BillboardLineNode | 存储面向屏幕的线段表达 | BillboardLineNodeDisplayManager |
| BillboardArrowNode | 存储面向屏幕的箭头表达 | BillboardArrowNodeDisplayManager |

## NodeBase 通用事实

各节点共享一组基础能力：

- nodeId：唯一标识，通常为无大括号 UUID
- nodeTagName：节点类型标识
- name、description：可读元数据
- attributeMap：通用属性存储
- referenceMap：引用关系存储
- version：修改版本号
- dirtyFlag：脏标记
- DisplayTarget：可见性与图层相关配置

## 节点修改规则

### 通用修改链路

```text
node.setXXX()
	-> touchModified()
	-> 节点事件发出
	-> SceneGraph 接收节点变化
	-> DisplayManager 根据事件类型更新内容/变换/显示
	-> VtkSceneWindow 渲染
```

### 批量修改

SceneGraph 提供批修改能力，语义如下：

- startBatchModify() 开始批操作
- endBatchModify() 结束批操作
- 只有批深度归零时才发出统一完成信号
- 批处理中产生的局部修改会合并，避免渲染层频繁抖动

### 删除规则

删除节点时必须：

1. 从 SceneGraph 容器移除节点
2. 断开节点事件连接
3. 清理父子引用和相关选择状态
4. 通知 DisplayManager 移除对应显示对象

## 显示目标与窗口映射

显示层使用多层 renderer，当前 VtkSceneWindow 会维护 3 层渲染层。

### 通用规则

- 节点可以根据 DisplayTarget 配置可见性和 layer
- 同一节点在不同窗口中可以存在不同显示目标配置
- DisplayManager 负责读取节点在当前窗口中的 layer 和 visible 设置

## VtkSceneWindow

VtkSceneWindow 是 VTK 渲染宿主，主要职责：

- 持有 QVTKOpenGLNativeWidget
- 持有 vtkGenericOpenGLRenderWindow
- 持有共享 camera
- 持有所有 NodeDisplayManager
- 在 reconcile() 中让每个 DisplayManager 同步 SceneGraph

### 当前典型窗口 ID

- datagen_main
- planning_main
- planning_overview
- navigation_main

## reconcile 流程

VtkSceneWindow::reconcile() 的核心语义不是“盲目重建整个场景”，而是“让每个 DisplayManager 以 SceneGraph 为真相执行幂等同步”。

### 流程

1. 遍历全部 DisplayManager。
2. 每个 DisplayManager 拉取自己关心的节点集合。
3. 清理已不存在节点的显示缓存。
4. 为新增节点构建显示条目。
5. 为已存在节点执行 updateContent()、updateDisplay()、updateTransform()。
6. 最终通过 render() 触发 VTK 绘制。

### 触发来源

- showEvent 首次显示
- 模块激活后的 requestReconcile()
- SceneGraph 节点变化后的渲染调度

## NodeDisplayManager 基类职责

NodeDisplayManager 负责：

- 过滤自己能处理的节点类型
- 维护 nodeId 到显示对象的缓存
- 响应 nodeAdded、nodeRemoved、nodeModified、batchModifyEnded
- 统一实现窗口可见性、图层和显示目标查询

## 各显示管理器差异

| 管理器 | 典型对象 | 差异点 |
| --- | --- | --- |
| PointNodeDisplayManager | 点、标签 | 点大小、标签、颜色数组 |
| LineNodeDisplayManager | 折线 | 顶点序列、闭合、线宽、虚线 |
| ModelNodeDisplayManager | 模型网格 | 材质参数多、标量着色、边缘显示 |
| PlaneNodeDisplayManager | 平面填充与边框 | 往往需要 fill 与 border 双对象 |
| BillboardLineNodeDisplayManager | 屏幕对齐线 | 世界到屏幕的 2D 表达 |
| BillboardArrowNodeDisplayManager | 屏幕对齐箭头 | 可能依赖相机方向动态调整 |
| TransformNodeDisplayManager | 坐标轴/变换可视化 | 坐标系可视化 |

## 世界变换同步

显示层对变换的标准做法是：

1. 从 SceneGraph 获取节点世界变换矩阵
2. 转换为 vtkMatrix4x4 或等价矩阵对象
3. 赋给 actor 的 user matrix 或等价接口

这意味着跨语言实现必须保留以下事实：

- TransformNode 树形层级存在
- 世界变换计算存在
- 显示对象不直接保存业务真相，只消费 SceneGraph 计算结果

## 渲染一致性检查点

实现者需要保证以下行为一致：

- 节点新增后能够出现在正确窗口和正确图层
- 节点删除后显示缓存能及时清理
- 批量修改只触发必要的渲染，而不是每次字段变化都全量重绘
- 模块激活后，相关 VTK 窗口能重新 reconcile 当前 SceneGraph
- 同一 SceneGraph 在多个 VTK 窗口里能以不同视角但一致数据真相显示

## 冻结项

- SceneGraph 作为唯一场景真相
- PointNode、LineNode、ModelNode、PlaneNode、TransformNode、BillboardLineNode、BillboardArrowNode 这些节点类型命名
- VtkSceneWindow::reconcile() 的职责边界
- PointNodeDisplayManager、LineNodeDisplayManager、ModelNodeDisplayManager、PlaneNodeDisplayManager、BillboardLineNodeDisplayManager、BillboardArrowNodeDisplayManager、TransformNodeDisplayManager 这些显示管理器命名

## 源码索引

- [../src/logic/scene/SceneGraph.h](../src/logic/scene/SceneGraph.h)
- [../src/ui/vtk3d/VtkSceneWindow.h](../src/ui/vtk3d/VtkSceneWindow.h)
- [../src/ui/vtk3d/VtkSceneWindow.cpp](../src/ui/vtk3d/VtkSceneWindow.cpp)
- [../src/display/NodeDisplayManager.h](../src/display/NodeDisplayManager.h)
- [../src/display/PointNodeDisplayManager.h](../src/display/PointNodeDisplayManager.h)
- [../src/display/LineNodeDisplayManager.h](../src/display/LineNodeDisplayManager.h)
- [../src/display/ModelNodeDisplayManager.h](../src/display/ModelNodeDisplayManager.h)
- [../src/display/PlaneNodeDisplayManager.h](../src/display/PlaneNodeDisplayManager.h)
- [../src/display/BillboardLineNodeDisplayManager.h](../src/display/BillboardLineNodeDisplayManager.h)
- [../src/display/BillboardArrowNodeDisplayManager.h](../src/display/BillboardArrowNodeDisplayManager.h)
- [../src/display/TransformNodeDisplayManager.h](../src/display/TransformNodeDisplayManager.h)

