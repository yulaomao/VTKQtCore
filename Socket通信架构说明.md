# Socket 通信架构说明

## 目标

当前应用运行路径已从 Redis 轮询/订阅切换为 socket 消息驱动：

```text
SocketClient 收到 JSON envelope
    -> CommunicationHub 统一解析
    -> LogicRuntime 按 module / targetModule 分发
    -> ModuleLogicHandler 更新数据与 SceneGraph
    -> LogicNotification 更新模块 UI / 全局 UI
```

UI 仍只负责布局、控件和用户操作采集；业务数据处理、节点更新和模块间路由集中在 logic 层。

## Socket 消息格式

推荐 envelope：

```json
{
  "module": "planning",
  "type": "state",
  "value": {
    "planPath": []
  }
}
```

- `module`：目标注册名。模块名可直接对应 `ModuleLogicRegistry` 中注册的 handler；`global`（大小写不敏感）会广播给所有模块。
- `type`：消息类型。
- `value`：业务负载。

兼容的控制类型：

- `action` / `action_request` / `ui_action`：转为 `LogicRuntime::onControlMessageReceived()`。
- `command` / `server_command`：转为 `LogicRuntime::onServerCommandReceived()`。
- `resync_request` / `resync_response`：触发统一重同步处理。
- `heartbeat`：更新通信健康状态。
- 其他类型：按 `StateSample` 投递给对应模块。

## UI 到 logic / socket

模块 UI 继续通过注入的 `UiActionDispatcher` 发送动作：

- `sendCommand()`：发给当前模块。
- `sendTargetedCommand()` / `sendToTarget()`：按目标注册名发送给指定模块。
- `sendModuleUiEvent()`：发模块间 UI 意图，由目标模块 logic 决定是否转发给自己的 widget。

当应用运行在 socket 模式时，`LocalLogicGateway` 会通过 `CommunicationHub` 将 UI 动作包装为 socket envelope，同时按本地 loopback 路由给 `LogicRuntime`，确保 UI 触发的本地状态更新不依赖服务端回包。

## 新软件复用边界

开发新界面时应优先只改这些位置：

1. 具体 `SoftwareInitializer`：决定启用模块、模块顺序、初始模块和 shell 扩展。
2. 模块 UI：页面布局、控件增删、样式和 widget 绑定。
3. 样式资源：全局 qss、模块 qrc、图标等。

尽量不要改：

- `CommunicationHub`
- `LogicRuntime`
- `ModuleLogicRegistry`
- `SceneGraph`
- 已稳定的模块 logic 数据处理规则

这样可以在功能、数据处理和节点更新保持不变的前提下，仅替换 UI 组合和样式。
