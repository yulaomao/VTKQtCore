# intermoduletest 模块规格

## 定位

intermoduletest 不是业务模块，而是用于验证模块间调用和模块 UI 事件的测试模块组。

## 组成

### Sender 侧

| 属性 | 值 |
| --- | --- |
| 模块 ID | intermoduletest_a |
| Handler 类 | InterModuleSenderLogicHandler |
| Widget | InterModuleSenderWidget |

### Receiver 侧

| 属性 | 值 |
| --- | --- |
| 模块 ID | intermoduletest_b |
| Handler 类 | InterModuleReceiverLogicHandler |
| Widget | InterModuleReceiverWidget |

## 主要目的

- 验证 ModuleInvokeRequest/ModuleInvokeResult 完整往返
- 验证模块 UI 事件 dispatch_module_ui_event
- 验证 CustomEvent 广播和模块间定向通信

## 关键常量

位于 [../../src/modules/intermoduletest/InterModuleTestConstants.h](../../src/modules/intermoduletest/InterModuleTestConstants.h)。

常见固定项包括：

- send_text_to_receiver
- display_text
- intermodule_text_updated

## 典型流程

1. Sender 发送 send_text_to_receiver。
2. Sender 通过 invokeModule() 调用 Receiver 的 display_text。
3. Receiver 返回 ModuleInvokeResult。
4. Receiver 再广播 intermodule_text_updated。
5. 相关 UI 部件即时更新。

## 最小验证路径

1. 启动应用。
2. 在 Sender 输入文本并发送。
3. 检查 Receiver 是否立即显示文本。
4. 检查 Sender 是否收到成功反馈或错误提示。

## 源码索引

- [../../src/modules/intermoduletest/InterModuleSenderLogicHandler.h](../../src/modules/intermoduletest/InterModuleSenderLogicHandler.h)
- [../../src/modules/intermoduletest/InterModuleSenderLogicHandler.cpp](../../src/modules/intermoduletest/InterModuleSenderLogicHandler.cpp)
- [../../src/modules/intermoduletest/InterModuleReceiverLogicHandler.h](../../src/modules/intermoduletest/InterModuleReceiverLogicHandler.h)
- [../../src/modules/intermoduletest/InterModuleReceiverLogicHandler.cpp](../../src/modules/intermoduletest/InterModuleReceiverLogicHandler.cpp)
- [../../src/modules/intermoduletest/InterModuleSenderWidget.h](../../src/modules/intermoduletest/InterModuleSenderWidget.h)
- [../../src/modules/intermoduletest/InterModuleReceiverWidget.h](../../src/modules/intermoduletest/InterModuleReceiverWidget.h)
