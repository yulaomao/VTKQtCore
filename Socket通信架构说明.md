# Socket 通信架构说明

## 目标

当前应用运行路径采用 socket 消息驱动：

```text
SocketClient 收到 JSON envelope
  -> CommunicationHub / LegacySocketAdapter 兼容旧 module/type/value envelope
  -> LogicRuntime 直接完成模块级路由
    -> ModuleLogicHandler 更新数据与 SceneGraph
    -> LogicNotification 更新模块 UI / shell / global-ui
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

- `module`：目标注册名。模块名可直接对应 `ModuleRuntimeRegistry`（当前由 `ModuleLogicRegistry` 承载）中注册的 handler 或 alias；`global`（大小写不敏感）会广播给所有模块。
- `type`：消息类型。
- `value`：业务负载。

兼容的控制类型：

- `action` / `action_request` / `ui_action`：由 `LegacySocketAdapter` 保持旧 envelope 兼容，再经 `LogicRuntime::onControlMessageReceived()` 转为本地 `UiAction` 路由。
- `command` / `server_command`：转为 `LogicRuntime::onServerCommandReceived()`，用于 shell 级命令和兼容控制面。
- `resync_request` / `resync_response`：触发统一重同步处理。
- `heartbeat`：更新通信健康状态。
- 其他类型：转换为 `StateSample` / `AppMessageKind::ExternalData` 后投递给目标模块。

## UI 到 logic / socket

模块 UI 继续通过注入的 `UiActionDispatcher` 发送动作：

- `sendCommand()`：发给当前模块。
- `sendTargetedCommand()` / `sendToTarget()`：按目标注册名发送给指定模块。
- `sendModuleUiEvent()`：发模块间 UI 意图，由目标模块 logic 决定是否转发给自己的 widget。

当应用运行在 socket 模式时，`UiActionDispatcher` 会先通过 `ILogicRuntimePort` 直接触发本地 `LogicRuntime`，同时把 action / resync 镜像发送给 `CommunicationHub`。这样既保留了外部协议兼容，又不再让本地 UI 主路径依赖 loopback。

## 新软件复用边界

开发新界面时应优先只改这些位置：

1. 具体 `SoftwareInitializer`：决定启用模块、模块顺序、初始模块和 shell 扩展。
2. 模块 UI：页面布局、控件增删、样式和 widget 绑定。
3. 样式资源：全局 qss、模块 qrc、图标等。

尽量不要改：

- `CommunicationHub` / `LegacySocketAdapter`
- `LogicRuntime` 的核心路由与 `ModuleLogicRegistry` 别名规则
- `ModuleLogicRegistry` 的运行时注册和 alias 规则
- `SceneGraph`
- 已稳定的模块 logic 数据处理规则

这样可以在功能、数据处理和节点更新保持不变的前提下，仅替换 UI 组合和样式。
