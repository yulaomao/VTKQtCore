# params 模块规格

## 基本信息

| 属性 | 值 |
| --- | --- |
| 模块 ID | params |
| Handler 类 | ParamsModuleLogicHandler |
| Page 类 | ParamsPage |
| UI 命令头 | ParamsUiCommands.h |
| 主要通知 | ButtonStateChanged, ModuleChanged |
| SceneGraph 影响 | 无 |

## 职责与边界

### 核心职责

- 采集和维护当前流程的参数
- 对参数进行合法性校验
- 向页面输出参数有效性和参数数量
- 支持外部状态样本同步参数

### 不负责

- 场景节点创建
- 规划或导航逻辑
- 复杂跨模块流程编排

## 输入契约

### 典型 UiAction 命令

- apply_parameters
- update_parameter

### StateSample

- 可接收外部参数状态样本并同步到内部参数表

## 输出契约

### 主要 LogicNotification

| 类型 | 目标 | 关键字段 |
| --- | --- | --- |
| ButtonStateChanged | CurrentModule 或 Shell/UI 路由后页面消费 | parametersValid, parameterCount |
| ModuleChanged | Shell | 参数变化快照 |

## 内部状态

- m_parameters，当前为 QVariantMap 形式的参数存储

## 页面与辅助区

### 主页面

- ParamsPage
- 三个核心输入区域：患者名、研究 ID、流程类型
- 按钮：应用、测试
- 状态标签：参数有效性与数量

### 辅助区

- 右侧摘要面板
- 用于显示参数状态和参数数量

## 典型工作流

1. 用户填写参数。
2. 页面收集参数并发送 apply_parameters。
3. ParamsModuleLogicHandler 校验参数。
4. 更新 m_parameters。
5. 发出 ButtonStateChanged 或相关通知。
6. 页面通过 setParameterStatus() 刷新状态。

## 最小验证路径

1. 进入 params。
2. 输入合法参数并点击应用。
3. 检查页面和右侧摘要是否显示“有效”。
4. 清空部分参数，再次应用。
5. 检查状态是否变为待检查或无效状态。

## 源码索引

- [../../src/modules/params/ParamsModuleLogicHandler.h](../../src/modules/params/ParamsModuleLogicHandler.h)
- [../../src/modules/params/ParamsModuleLogicHandler.cpp](../../src/modules/params/ParamsModuleLogicHandler.cpp)
- [../../src/modules/params/ParamsPage.h](../../src/modules/params/ParamsPage.h)
- [../../src/modules/params/ParamsPage.cpp](../../src/modules/params/ParamsPage.cpp)
- [../../src/modules/params/ParamsUiCommands.h](../../src/modules/params/ParamsUiCommands.h)
