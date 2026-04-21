# 模块间 UI 事件接口说明

这份文档说明当前工程里新增的这套接口：

`模块 A 的按钮 -> UiActionDispatcher::sendModuleUiEvent() -> LogicRuntime 路由 -> 模块 B logic 转发 -> 模块 B widget 更新`

它的目标不是让模块 A 直接拿到模块 B 的 widget 指针并调用成员函数，而是提供一条**轻量、可约束、可追踪**的跨模块 UI 事件通道。

---

## 1. 为什么要加这套接口

如果直接做成：

- `moduleAWidget->call(moduleBWidget->setText(...))`
- `invokeWidget("moduleB", "setText", args)`

短期看方便，长期会引出几个问题：

1. widget 生命周期耦合严重
2. 模块是否激活、页面是否已装配都变成调用方负担
3. 接口会退化成字符串反射，调试困难
4. 模块边界被 UI 细节污染

因此这里采用的是：

- **发送方只描述一个 UI 意图**
- **目标模块 logic 决定是否转发给自己的 widget**
- **目标 widget 只订阅自己关心的 eventName**

---

## 2. 当前新增的几个核心接口

### 2.1 `ModuleUiEvent` 协议

位置：

- `src/contracts/ModuleUiEvent.h`

职责：

- 统一 UI 事件 command 名称
- 统一 notification payload 结构
- 提供 action / notification 的判断与提取函数

核心约定：

```cpp
command      = "dispatch_module_ui_event"
eventCategory = "module_ui_event"
eventName     = 具体事件名
sourceModule  = 事件来源模块
```

---

### 2.2 发送端接口 `sendModuleUiEvent`

位置：

- `src/ui/coordination/UiActionDispatcher.h`
- `src/ui/coordination/UiActionDispatcher.cpp`

新增接口：

```cpp
void sendModuleUiEvent(const QString& targetModule,
                       const QString& eventName,
                       const QVariantMap& payload = {});
```

它会自动构造一条 `UiAction::CustomAction`，并在 payload 里带上：

- `command = dispatch_module_ui_event`
- `targetModule = ...`
- `eventName = ...`

发送方只需要关心：

1. 目标模块是谁
2. 事件名是什么
3. 额外参数是什么

---

### 2.3 logic 侧统一转发 helper

位置：

- `src/logic/registry/ModuleLogicHandler.h`
- `src/logic/registry/ModuleLogicHandler.cpp`

新增 helper：

```cpp
bool forwardModuleUiEventAction(const UiAction& action,
                                const QString& sourceModule = QString());

void emitModuleUiEvent(const QString& eventName,
                       const QVariantMap& payload = {},
                       const QString& sourceModule = QString(),
                       const QString& sourceActionId = QString(),
                       LogicNotification::TargetScope scope = LogicNotification::ModuleList,
                       const QStringList& targetModules = {});
```

其中：

- `forwardModuleUiEventAction()` 用来判断当前 action 是否是 UI 事件，如果是，就直接转成模块通知
- `emitModuleUiEvent()` 用来在模块内部主动发一个统一结构的 UI 事件通知

这样每个模块 logic 不需要重复写：

- `if command == dispatch_module_ui_event`
- `if eventName == ...`
- 手动拼 `LogicNotification::CustomEvent`

---

### 2.4 widget 侧统一绑定 helper

位置：

- `src/ui/coordination/ModuleUiEventBinding.h`

接口：

```cpp
ModuleUiEventBinding::bind(
    gateway,
    moduleId,
    eventName,
    receiver,
    [](const QVariantMap& payload) {
        ...
    });
```

它负责统一过滤：

1. `notification.eventType == CustomEvent`
2. `notification.targetScope` 是否命中当前模块
3. `eventCategory == module_ui_event`
4. `eventName` 是否匹配

widget 只收到最终净化后的业务 payload。

---

## 3. 一条完整调用链是怎么走的

### 第 1 步：模块 A 的按钮发送 UI 事件

```cpp
m_actionDispatcher->sendModuleUiEvent(
    QStringLiteral("module_b"),
    QStringLiteral("preview_text"),
    {{QStringLiteral("text"), text}});
```

### 第 2 步：`LogicRuntime` 按 `targetModule` 路由 action

这一步仍然走现有运行时路由，不需要额外改 `LogicRuntime`。

### 第 3 步：模块 B 的 logic 决定是否转发

```cpp
void ModuleBLogicHandler::handleAction(const UiAction& action)
{
    if (forwardModuleUiEventAction(action)) {
        return;
    }

    // 继续处理本模块其他 command
}
```

### 第 4 步：模块 B 的 widget 绑定自己关心的事件

```cpp
ModuleUiEventBinding::bind(
    gateway,
    QStringLiteral("module_b"),
    QStringLiteral("preview_text"),
    this,
    [this](const QVariantMap& payload) {
        const QString text = payload.value(QStringLiteral("text")).toString();
        updatePreview(text);
    });
```

---

## 4. 发送端怎么写

发送端一般在 page/widget 内部。

前提：

- 这个 widget 已经拿到了自己的 `UiActionDispatcher*`

示例：

```cpp
void SenderWidget::sendPreview()
{
    if (!m_actionDispatcher) {
        return;
    }

    const QString text = m_input->text().trimmed();
    if (text.isEmpty()) {
        return;
    }

    m_actionDispatcher->sendModuleUiEvent(
        QStringLiteral("receiver"),
        QStringLiteral("preview_text"),
        {{QStringLiteral("text"), text}});
}
```

建议：

1. `eventName` 用模块自己定义的常量，不要到处写裸字符串
2. payload 只传 widget 真正需要的数据
3. 如果这是临时 UI 展示，不要顺手写 Redis 或持久状态

---

## 5. 目标模块 logic 怎么写

如果这个模块希望支持跨模块 UI 事件，最简单的写法就是在 `handleAction()` 开头加：

```cpp
void ReceiverLogicHandler::handleAction(const UiAction& action)
{
    if (forwardModuleUiEventAction(action)) {
        return;
    }

    // 其他 command 逻辑
}
```

这个写法适用于：

- 目标模块只需要把 UI 事件原样转给自己的 widget

如果你还想做权限控制、模块状态检查、数据清洗，也可以不用这个 helper，而是在 logic 里自己识别 `ModuleUiEvent::isAction(action)` 后再决定怎么处理。

---

## 6. 目标 widget 怎么写

widget 不需要知道 action、runtime、targetModule 路由细节。

只需要绑定：

```cpp
ModuleUiEventBinding::bind(
    gateway,
    moduleId,
    eventName,
    this,
    [this](const QVariantMap& payload) {
        ...
    });
```

示例：

```cpp
ModuleUiEventBinding::bind(
    gateway,
    QStringLiteral("receiver"),
    QStringLiteral("preview_text"),
    this,
    [this](const QVariantMap& payload) {
        const QString text = payload.value(QStringLiteral("text")).toString();
        if (!text.isEmpty()) {
            m_previewLabel->setText(text);
        }
    });
```

这意味着：

- widget 只订阅自己关心的事件名
- 不需要手写 targetScope / targetModules 判断
- 不需要自己判断 eventCategory

---

## 7. intermoduletest 示例说明

当前示例位置：

- `src/modules/intermoduletest/InterModuleSenderWidget.cpp`
- `src/modules/intermoduletest/InterModuleReceiverWidget.cpp`
- `src/modules/intermoduletest/InterModuleReceiverLogicHandler.cpp`
- `src/modules/intermoduletest/InterModuleSenderLogicHandler.cpp`
- `src/modules/intermoduletest/InterModuleTestConstants.h`

这个示例现在同时演示了两条链路。

### 7.1 UI 事件预览链路

用途：

- 演示“模块 A 按钮触发模块 B widget 的界面更新”
- 不走持久业务状态

#### 模块 A 发送

`InterModuleSenderWidget` 里新增按钮：

- `UI 事件预览`

点击后执行：

```cpp
m_actionDispatcher->sendModuleUiEvent(
    InterModuleTest::receiverModuleId(),
    InterModuleTest::previewTextEvent(),
    {{QStringLiteral("text"), text}});
```

#### 模块 B logic 转发

`InterModuleReceiverLogicHandler::handleAction()` 开头：

```cpp
if (forwardModuleUiEventAction(action)) {
    return;
}
```

#### 模块 B widget 接收

`InterModuleReceiverWidget` 在构造时绑定：

```cpp
ModuleUiEventBinding::bind(
    gateway,
    InterModuleTest::receiverModuleId(),
    InterModuleTest::previewTextEvent(),
    this,
    [this](const QVariantMap& payload) {
        setPreviewText(payload.value(QStringLiteral("text")).toString());
    });
```

最终效果：

- A 点按钮
- B 的“UI 事件预览”标签立即更新

---

### 7.2 Logic 持久发送链路

用途：

- 保留原来的模块间 logic 调用示例
- 演示“跨模块业务逻辑”与“跨模块 UI 事件”的区别

#### 模块 A 发送 command

`InterModuleSenderWidget` 的另一个按钮：

- `Logic 持久发送`

点击后执行：

```cpp
m_actionDispatcher->sendCommand(
    InterModuleTest::sendTextCommand(),
    {{QStringLiteral("text"), text}});
```

#### 模块 A logic 调模块 B logic

`InterModuleSenderLogicHandler` 使用：

```cpp
invokeModule(
    InterModuleTest::receiverModuleId(),
    InterModuleTest::displayTextMethod(),
    {{QStringLiteral("text"), text},
     {QStringLiteral("sourceActionId"), action.actionId}});
```

#### 模块 B logic 更新状态并发通知

`InterModuleReceiverLogicHandler::displayText()` 更新内部文本后，发送：

- `receiverTextUpdatedEvent()`

最终效果：

- A 点按钮
- B 的“Logic 持久文本”标签更新

---

## 8. 两条链路该怎么选

### 用 `sendModuleUiEvent()` 的场景

适合：

- 临时 UI 联动
- 预览、聚焦、闪烁、高亮、展开面板
- 不需要持久保存到模块状态的数据

例如：

- A 请求 B 高亮某个节点
- A 请求 B 展开某个面板
- A 请求 B 预览一段文本

### 用 `invokeModule()` 的场景

适合：

- 正式业务动作
- 目标模块需要改变内部状态
- 需要校验、失败处理、返回结果

例如：

- A 请求 B 保存表单
- A 请求 B 生成规划
- A 请求 B 更新业务模型

一句话区分：

> **临时 UI 联动优先走 `sendModuleUiEvent()`；正式跨模块业务协作优先走 `invokeModule()`。**

---

## 9. 推荐约定

为了后续维护更清楚，建议遵守下面几条。

### 9.1 eventName 放到模块常量里

例如：

```cpp
inline QString previewTextEvent()
{
    return QStringLiteral("intermodule_preview_text");
}
```

不要在 page、logic、widget 三处重复写裸字符串。

### 9.2 payload 只放 widget 需要的数据

不建议把整份业务状态都塞进 UI 事件。

例如预览文本只需要：

```cpp
{{QStringLiteral("text"), text}}
```

### 9.3 不要把 UI 事件当成业务状态同步通道

如果一个数据需要：

- 可恢复
- 可重放
- 可参与业务计算

那它应该走模块状态或 `invokeModule()`，不要只发一个 UI 事件。

### 9.4 接收侧尽量通过 binding helper 订阅

不要每个 widget 都重复写：

- `if notification.eventType != CustomEvent`
- `if notification.targetScope != ...`
- `if notification.payload["eventName"] != ...`

统一放到 `ModuleUiEventBinding::bind()` 里更稳。

---

## 10. 最小模板

如果你要在新模块里复用这套方案，最小模板如下。

### 发送端

```cpp
m_actionDispatcher->sendModuleUiEvent(
    QStringLiteral("target_module"),
    QStringLiteral("focus_node"),
    {{QStringLiteral("nodeId"), nodeId}});
```

### 目标模块 logic

```cpp
void TargetLogicHandler::handleAction(const UiAction& action)
{
    if (forwardModuleUiEventAction(action)) {
        return;
    }

    // other actions...
}
```

### 目标 widget

```cpp
ModuleUiEventBinding::bind(
    gateway,
    QStringLiteral("target_module"),
    QStringLiteral("focus_node"),
    this,
    [this](const QVariantMap& payload) {
        const QString nodeId = payload.value(QStringLiteral("nodeId")).toString();
        focusNode(nodeId);
    });
```

---

## 11. 一句话总结

如果你想实现“模块 A 的按钮触发模块 B 的 widget 中某个界面行为”，推荐做法不是直接调用对方 widget，而是：

> **A 用 `sendModuleUiEvent()` 发送一个目标模块 UI 事件，B 的 logic 用 `forwardModuleUiEventAction()` 转发，B 的 widget 用 `ModuleUiEventBinding::bind()` 订阅并执行自己的界面更新。**

这样接口更稳定，模块边界更清晰，也更符合当前框架的 action / logic / notification 分层。