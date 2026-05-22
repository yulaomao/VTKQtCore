# UI Framework Guide

## 目标

本项目是一个 Qt + VTK + socket 消息驱动的模块化客户端框架。UI 层负责布局、控件和用户动作采集；logic 层负责业务判断、模块路由、SceneGraph 更新和通知分发；通信层负责 socket envelope 的收发与标准化。

## 总体分层

```text
MainWindow / WorkspaceShell
    -> ApplicationCoordinator / ModuleCoordinator
    -> LogicRuntime / AppMessageCenter / ModuleLogicHandler
    -> SceneGraph / DisplayManager
    -> CommunicationHub / LegacySocketAdapter / SocketClient
```

各层职责：

- UI：只发出 `UiAction`，只消费 `LogicNotification`，不直接操作 SceneGraph。
- Coordinator：绑定页面控件、模块状态和通知，不承载业务规则。
- LogicRuntime：统一接收 UI 动作、服务端命令、外部状态样本和模块间调用。
- ModuleLogicHandler：模块业务入口，处理动作、状态样本和模块内部调用。
- SceneGraph：保存场景节点和显示目标，是 3D 渲染的数据真相。
- CommunicationHub：维护 socket 连接，把外部 envelope 转成客户端内部消息。

## Socket Envelope

推荐格式：

```json
{
  "module": "planning",
  "type": "state",
  "value": {
    "planPath": []
  }
}
```

- `module`：目标模块注册名。`global` 会广播给所有模块。
- `type`：消息类型，例如 `state`、`action_request`、`server_command`、`resync_request`、`heartbeat`。
- `value`：业务负载。

## UI 到 Logic

模块页面通过 `UiActionDispatcher` 发动作：

- `sendCommand()` 发给当前模块。
- `sendTargetedCommand()` 发给指定模块。
- `sendModuleUiEvent()` 发模块间 UI 意图。

`LocalLogicGateway` 会把动作送入 `CommunicationHub`，再通过本地 loopback 进入 `LogicRuntime`，让用户操作在无服务端回包时也能完成本地状态更新。

## Logic 到 UI

模块 logic 通过 `LogicNotification` 通知 UI：

- `SceneNodesUpdated`：SceneGraph 或模块状态变化。
- `ButtonStateChanged`：按钮、表单或模块控制状态变化。
- `ModuleChanged` / `ActiveModuleChanged`：当前模块变化。
- `ConnectionStateChanged`：通信状态变化。
- `ErrorOccurred`：可恢复或不可恢复错误。

通知由 `ApplicationCoordinator`、`ModuleCoordinator`、`GlobalUiManager` 按目标范围分发。

## SceneGraph 规则

- 所有业务节点更新先进入 SceneGraph，再由显示层刷新 actor。
- UI 不直接改 vtkActor，也不直接修改业务节点。
- 外部坐标、模型、点线面数据进入客户端后必须先经过模块 logic 解释，再写入对应 Node。
- 节点显示策略应落到 DisplayTarget 或节点属性，不绕过 SceneGraph。

## 新软件接入

开发一个新软件组合时优先修改：

1. 具体 `SoftwareInitializer`：模块集合、模块顺序、初始模块和 shell 扩展。
2. 模块 logic：业务命令、状态样本和 SceneGraph 写入规则。
3. 模块 UI：页面控件、布局、样式和动作绑定。
4. 样式与资源：qss、qrc、图标和提示音资源。

尽量不要改：

- `CommunicationHub` / `LegacySocketAdapter`
- `LogicRuntime` / `AppMessageCenter`
- `ModuleLogicRegistry` 的注册和 alias 规则
- 已稳定的 SceneGraph 节点基础类型

## 验证建议

- 启动应用后检查连接状态通知。
- 通过 UI 触发模块切换，确认 `ActiveModuleState` 与页面同步。
- 向 socket 服务发送 state envelope，确认对应模块 handler 收到 `StateSample`。
- 触发 DataGen 节点创建、删除和显示修改，确认 SceneGraph 与 UI 同步刷新。
- 触发 resync 请求，确认各模块执行自己的 `onResync()` 且不会访问外部持久层。
