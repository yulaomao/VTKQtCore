# Redis 通讯说明

这份文档对应当前代码里的真实实现：

> 一次 MGET 轮询，只做一次按模块聚合下发。模块收到的是本轮完整的 `QVariantMap values`，模块内部自己再分类处理。

当前默认主链路是：

`redis_dispatch_config.json -> RedisDataCenter -> LogicRuntime -> ModuleLogicHandler::handleStateSample()`

旧的 `CommunicationHub -> parser -> LogicRuntime::onStateSampleReceived()` 链路仍然保留，但不是当前 Redis polling 的默认入口。

---

## 1. 开发者平时只需要看三层

### 1.1 配置层

位置：

- `src/app/resources/redis_dispatch_config.json`

这里决定：

- 连接哪个 Redis
- 每条连接轮询哪些 key
- 每组 key 属于哪个模块
- 订阅哪些 channel
- 每个 channel 属于哪个模块

如果你只是新增 Redis 数据来源，通常先改这里。

### 1.2 中转层

位置：

- `src/communication/redis/RedisDataCenter.cpp`
- `src/logic/runtime/LogicRuntime.cpp`

这里负责：

- worker 返回一轮 MGET 结果后，先做 JSON 归一化
- 按 key 找到所属模块
- 把同一模块的数据聚合成一个 `QVariantMap`
- 每个模块本轮只调用一次 `handleStateSample()`

如果只是加 key 或改模块处理逻辑，通常不用改这里。

### 1.3 模块层

位置通常在：

- `src/modules/params/ParamsModuleLogicHandler.cpp`
- `src/modules/planning/PlanningModuleLogicHandler.cpp`
- `src/modules/pointpick/PointPickModuleLogicHandler.cpp`
- `src/modules/navigation/NavigationModuleLogicHandler.cpp`

这里才是业务真正落地的地方。

模块现在要显式从 `sample.data["values"]` 读取 polling 聚合数据，而不是再依赖单条 `key/value` 兼容字段。

---

## 2. 当前 polling 链路是怎么走的

### 第 1 步：启动 RedisDataCenter

位置：

- `src/app/software/ConcreteSoftwareInitializers/DefaultSoftwareInitializer.cpp`

启动时会：

1. 从 `:/redis_dispatch_config.json` 读取 `RedisConnectionConfig`
2. 创建 `RedisDataCenter(configs, runtime)`
3. 调 `start()`

当前 `registerCommunicationSources()` 已经把 Redis polling / subscription 的主责任放到了 `RedisDataCenter`，不是 `CommunicationHub`。

### 第 2 步：每个连接一个 worker

位置：

- `src/communication/redis/RedisDataCenter.cpp`

`RedisDataCenter` 会为每个连接配置创建一个 `RedisConnectionWorker`。

每个 worker 负责：

- 连接目标 Redis
- 周期性执行一轮 MGET
- 接收订阅消息

这意味着当前是“每个连接一个 worker”，不是“所有连接共用一个全局轮询器”。

### 第 3 步：一轮 MGET 读回所有 key

轮询 key 来自：

- `RedisConnectionConfig::pollingKeyGroups`

示意配置：

```json
{
  "connectionId": "conn_main",
  "pollingKeyGroups": [
    { "module": "params", "keys": ["state.params.latest"] },
    { "module": "planning", "keys": ["state.planning.latest"] },
    { "module": "navigation", "keys": ["state.navigation.latest", "state.navigation.transforms"] }
  ]
}
```

这里的关键点是：

- worker 一次轮询会批量取回所有配置 key
- key 属于哪个模块，配置里已经提前声明好了

### 第 4 步：RedisDataCenter 按模块聚合

位置：

- `src/communication/redis/RedisDataCenter.cpp`

`RedisDataCenter::onPollBatch(connectionId, rawValues)` 当前做的是：

1. 找到 `connectionId` 对应配置
2. 遍历本轮 MGET 返回的全部 key/value
3. 调 `normalizeValue()` 把 JSON 字符串解成 `QVariant` / `QVariantMap`
4. 用 `moduleForKey(key)` 找到模块归属
5. 按模块把本轮数据聚合成：

```cpp
moduleBatches[module][key] = normalizedValue;
```

6. 每个模块只调用一次：

```cpp
LogicRuntime::onModulePollBatch(module, values)
```

这里已经没有“本轮读到 10 个 key，就给模块回调 10 次”的逻辑了。

### 第 5 步：LogicRuntime 再包装成一条聚合 sample

位置：

- `src/logic/runtime/LogicRuntime.cpp`

`LogicRuntime::onModulePollBatch()` 当前会把该模块本轮完整数据包装成：

```cpp
data["values"] = values;
```

然后构造一条 `poll_batch` 类型的 `StateSample`，再调用：

```cpp
handler->handleStateSample(sample)
```

如果模块配置为 `global`，则会把同一份聚合 `values` 广播给所有已注册模块。

重点是：

- runtime 不再按 key 逐条转发
- runtime 也不再给单条轮询数据额外塞 `key/value` 兼容字段
- polling 模块统一从 `sample.data["values"]` 读本轮聚合包

### 第 6 步：模块内部自己拆分和处理

模块收到的 `sample.data["values"]` 结构是：

```cpp
{
    "state.navigation.latest": {...},
    "state.navigation.transforms": {...},
    "state.navigation.guide": {...}
}
```

模块内部自己决定怎么处理：

- 如果模块只有一个 key，可以直接取 `values` 里唯一那项的 payload
- 如果模块有多个 key，可以遍历 `values`，按 key 或 payload 内容分类处理
- 如果多个 key 对应 payload 都是 map，也可以先合并再处理

当前代码里的实践是：

- `navigation` 遍历 `values`，分别处理导航状态和 transform
- `planning`、`pointpick`、`params` 从 `values` 聚合出本轮业务 payload，再继续原本逻辑

所以模块开发者真正要写的入口，仍然是：

```cpp
void handleStateSample(const StateSample& sample)
```

只是现在对于 polling，你应该先看：

```cpp
sample.data.value("values").toMap()
```

---

## 3. 当前 subscription 链路

subscription 没改成批量聚合，它仍然是单消息直达。

### 第 1 步：配置订阅频道

位置：

- `src/app/resources/redis_dispatch_config.json`

例如：

```json
"subscriptionChannels": [
  { "channel": "state.navigation", "module": "navigation" }
]
```

### 第 2 步：worker 收到消息后上抛

会进入：

- `RedisDataCenter::onSubscription(connectionId, module, channel, payload)`

### 第 3 步：runtime 直接分发给模块

位置：

- `src/communication/redis/RedisDataCenter.cpp`
- `src/logic/runtime/LogicRuntime.cpp`

当前订阅路径是：

```cpp
LogicRuntime::onModuleSubscription(module, channel, payload)
```

然后模块收到：

```cpp
handleSubscription(channel, payload)
```

默认实现仍然会把订阅消息包成一条 `StateSample` 再流入 `handleStateSample()`，但这和 polling 不同，订阅消息没有 `values` 这一层批量聚合壳。

---

## 4. 开发者该怎么用

### 场景 1：新增一个 Redis polling key

做法：

1. 在 `src/app/resources/redis_dispatch_config.json` 里把 key 挂到对应模块
2. 到对应模块的 `*ModuleLogicHandler.cpp` 里改 `handleStateSample()`
3. 从 `sample.data["values"]` 读取本轮该模块的聚合数据
4. 在模块内部按 key 或 payload 自己分类处理

通常不需要改 `RedisDataCenter`。

### 场景 2：一个模块要同时消费多个 polling key

做法：

1. 把多个 key 都配置到同一个模块
2. 在模块的 `handleStateSample()` 里遍历 `values`
3. 一次 handler 调用内完成聚合、分类、状态更新和通知发送

这就是当前推荐方式。

不要再假设：

- 每个 key 会单独触发一次 handler
- 单条轮询数据会自动以 `sample.data["value"]` 给你

### 场景 3：改中转规则

只有在这些情况才需要动中转层：

- 要改 key 到 module 的路由逻辑
- 要改 polling 数据归一化方式
- 要改全局广播策略

对应位置：

- `src/communication/redis/RedisDataCenter.cpp`
- `src/logic/runtime/LogicRuntime.cpp`

---

## 5. 一句话结论

当前 Redis polling 的实现原则是：

> 一轮 MGET，先在 `RedisDataCenter` 按模块聚合；再在 `LogicRuntime` 每模块只下发一次；模块统一从 `sample.data["values"]` 读取本轮完整数据并自行处理。

如果你要写 Redis 业务逻辑，优先改：

- 配置文件
- 对应模块的 `handleStateSample()`

不是先去改旧的 parser 链，也不是假设框架会按单 key 多次回调模块。

### 第 5 步：模块默认仍会落到 `handleStateSample()`

位置：

- `src/logic/registry/ModuleLogicHandler.cpp`

和 polling 一样，如果模块没有自己重写 `handleSubscription()`，框架会把它包装成：

```cpp
StateSample sample;
sample.sampleType = "subscription";
sample.data = payload + { "channel": channel };
```

然后继续调：

- `handleStateSample(sample)`

所以很多模块完全不需要区分“这条数据是 polling 来的还是 subscription 来的”，只需要看自己能不能从 payload 里取到要的字段。

---

## 5. 开发者应该在哪写代码

这是最重要的部分。

### 5.1 想新增一个轮询 key

通常只需要做两件事：

1. 在 `src/app/resources/redis_dispatch_config.json` 里把 key 加到对应模块的 `pollingKeyGroups`
2. 在对应模块的 `ModuleLogicHandler` 里处理这个 key 对应的数据

例如你想让 `planning` 模块接收一个新的状态 key：

- 配置里加到 `planning` 对应的 group
- 在 `PlanningModuleLogicHandler::handleStateSample()` 里读取 `sample.data["value"]`

### 5.2 想新增一个订阅 channel

通常也只需要两件事：

1. 在 `src/app/resources/redis_dispatch_config.json` 里加一条 `subscriptionChannels`
2. 在对应模块 handler 里处理这条频道发来的 payload

### 5.3 想新增一个模块接收 Redis 数据

一般要做三件事：

1. 先在 `DefaultSoftwareInitializer::registerModuleLogicHandlers()` 里注册该模块 handler
2. 在 `redis_dispatch_config.json` 里把 key / channel 绑定到这个模块
3. 在该模块的 logic handler 里实现处理逻辑

### 5.4 想修改“数据该分给哪个模块”的规则

先看是不是配置就能解决。

如果只是：

- 某个 key 应该给另一个模块
- 某个 channel 应该改归属

那改：

- `src/app/resources/redis_dispatch_config.json`

就够了。

只有当你想改框架分发方式本身，例如：

- 一个 key 不再只给一个模块
- 需要按 payload 内容再二次路由
- 想在中转层统一做特殊结构转换

才需要改：

- `src/communication/redis/RedisDataCenter.cpp`

### 5.5 想在模块里直接读写 Redis

直接在模块 handler 里用 `ModuleLogicHandler` 提供的保护方法即可：

- `readRedisValue`
- `readRedisJsonValue`
- `writeRedisValue`
- `writeRedisJsonValue`
- `publishRedisMessage`
- `publishRedisJsonMessage`

这条链路和 polling / subscription 分发是并列关系，不需要经过 `RedisDataCenter` 中转。

---

## 6. 当前模块代码通常怎么写

当前大多数模块还是走兼容入口：

- `handleStateSample(const StateSample& sample)`

例如：

- `ParamsModuleLogicHandler` 会从 `sample.data["parameters"]`、`sample.data["value"]` 或 `key/value` 包装里取参数
- `PlanningModuleLogicHandler` 会从 `sample.data["value"]` 里取 `vertices`、`triangles`、`path`、`status`、`accepted`
- `PointPickModuleLogicHandler` 会从 `sample.data["value"]` 里取 `points`、`confirmed`、`selectedIndex`
- `NavigationModuleLogicHandler` 会从 `sample.data["value"]` 或 payload 顶层取 `nodeId`、`matrixToParent`、`status`、`navigating`

所以当前推荐做法不是一上来就改 framework，而是：

> 先让 Redis key 的 value 结构尽量贴近模块 handler 已经在消费的结构。

这样改动最小。

如果你后面想把模块写得更干净，可以再逐步把模块从 `handleStateSample()` 迁移到：

- `handlePollData(const QString& key, const QVariant& value)`
- `handleSubscription(const QString& channel, const QVariantMap& payload)`

但这不是当前必须动作。

---

## 7. `global` 模块有什么用

当前配置和 runtime 都支持一个特殊模块名：

- `global`

含义是：

- 这条 polling key 或 subscription channel 不只给一个模块
- 而是广播给所有已注册模块 handler

适合用于：

- 全局状态
- 所有模块都要感知的共享消息
- 不方便提前静态归属到单模块的数据

但要注意：

> `global` 只是广播，不做业务过滤。真正是否处理，仍由各模块自己决定。

---

## 8. 哪些文件通常不用改

正常业务开发时，下面这些文件通常不需要改：

- `src/logic/runtime/LogicRuntime.cpp`
- `src/communication/hub/CommunicationHub.cpp`
- `src/logic/runtime/GlobalPollingSampleParser.h`
- `src/app/software/DefaultGlobalPollingSampleParser.cpp`

原因很简单：

- 当前主数据链路已经走 `RedisDataCenter`
- 旧的 global polling sample parser 路径不是默认入口

只有在你要重构框架层数据分发方式、兼容旧链路、或者做非常特殊的中转策略时，才需要碰这些文件。

---

## 9. 开发者最常见的使用方式

### 场景 1：模块要轮询一个 Redis key

做法：

1. 在 `redis_dispatch_config.json` 的对应 connection 下，把 key 加入某个模块的 `pollingKeyGroups`
2. 启动后该 key 会被 MGET 读到
3. `RedisDataCenter` 会把它转给该模块
4. 在模块 handler 里处理 `sample.data["value"]`

### 场景 2：模块要监听一个 Redis channel

做法：

1. 在 `subscriptionChannels` 里新增 channel 和 module 的映射
2. 启动后 worker 会自动订阅
3. 消息到达后直接送到模块 handler

### 场景 3：模块要主动写回 Redis

做法：

1. 在模块 handler 里调用 `writeRedisValue` / `writeRedisJsonValue`
2. 如果要广播消息，调用 `publishRedisMessage` / `publishRedisJsonMessage`

### 场景 4：模块拿到的数据结构不合适

优先顺序建议是：

1. 先看 Redis 里的 value 能不能直接改成更适合模块消费的结构
2. 再看模块 handler 是否可以兼容当前结构
3. 最后才去改 `RedisDataCenter` 的中转规则

---

## 10. 一句话总结

基于当前代码，开发者可以这样理解 Redis 通讯：

> 平时先在 `src/app/resources/redis_dispatch_config.json` 里声明“从哪个连接轮询 / 订阅什么数据、这些数据归哪个模块”，然后在对应模块的 `ModuleLogicHandler` 里写最终处理逻辑；只有当默认分发方式不够用时，才需要去 `src/communication/redis/RedisDataCenter.cpp` 改中转逻辑。