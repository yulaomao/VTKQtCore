# Redis 通讯说明

这份文档说明当前项目里的 Redis 数据链路，以及开发者平时应该在哪里写代码。

如果只看一句话，可以这样理解：

> 当前主链路是 `redis_dispatch_config.json -> RedisDataCenter -> LogicRuntime -> ModuleLogicHandler`。开发时通常只需要改配置、写模块处理逻辑，必要时再补中转规则。

---

## 1. 先看当前真实链路

当前默认 Redis 数据路径不是旧版文档里的：

- `CommunicationHub -> global_poll_batch -> parser -> LogicRuntime`

而是：

- `DefaultSoftwareInitializer` 启动 `RedisDataCenter`
- `RedisDataCenter` 为每个 Redis 连接创建一个 `RedisConnectionWorker`
- worker 周期性 `MGET`，或接收 pub/sub
- `RedisDataCenter` 按配置把 key / channel 分发给目标模块
- `LogicRuntime` 把数据转交给对应 `ModuleLogicHandler`
- 模块自己的 handler 最终处理数据

也就是说，当前主路径已经不再依赖 `GlobalPollingSampleParser` 作为核心中转点。

`GlobalPollingSampleParser`、`DefaultGlobalPollingSampleParser`、`LogicRuntime::onStateSampleReceived()` 这条链还保留着，但现在更多是兼容旧路径，不是默认开发入口。

---

## 2. 开发者平时只需要关心哪三层

日常开发一般只关心下面三层：

### 2.1 配置层：决定从哪个 Redis 连接读什么

位置：

- `src/app/resources/redis_dispatch_config.json`

这里定义：

- Redis 连接信息
- 每条连接轮询哪些 key
- 每组 key 属于哪个模块
- 每条连接订阅哪些 channel
- 每个 channel 属于哪个模块

这是开发者最常改的地方。

### 2.2 中转层：把 Redis 数据送到哪个模块

位置：

- `src/communication/redis/RedisDataCenter.cpp`

这里负责：

- 接收 worker 的 MGET 结果
- 对 value 做 JSON 解码归一化
- 根据配置把 key 分发到模块
- 把订阅消息分发到模块

如果只是新增 key 或 channel，通常不用改这里。

只有当你要改“中转规则”本身时，才需要动这里。

### 2.3 模块层：最终业务处理

位置通常在：

- `src/modules/params/ParamsModuleLogicHandler.cpp`
- `src/modules/planning/PlanningModuleLogicHandler.cpp`
- `src/modules/pointpick/PointPickModuleLogicHandler.cpp`
- `src/modules/navigation/NavigationModuleLogicHandler.cpp`

这里才是你真正写业务逻辑的地方。

---

## 3. 当前 polling 数据是怎么进模块的

下面按当前代码真实执行顺序说明一条轮询数据的流动。

### 第 1 步：初始化器启动 RedisDataCenter

位置：

- `src/app/software/ConcreteSoftwareInitializers/DefaultSoftwareInitializer.cpp`

`DefaultSoftwareInitializer::configureAdditionalSettings()` 当前做的是：

1. 从 `:/redis_dispatch_config.json` 读取连接配置
2. 创建 `RedisDataCenter(configs, runtime)`
3. 调 `dataCenter->start()`

当前 `registerCommunicationSources()` 里已经明确写了：

- 数据 polling 和 subscription 现在由 `RedisDataCenter` 管
- `CommunicationHub` 这边不再负责数据 polling 主链

所以今天再加 Redis 数据源，首先想到的不是 `CommunicationHub`，而是：

- 配置文件
- `RedisDataCenter`

### 第 2 步：每个连接有一个 worker

位置：

- `src/communication/redis/RedisDataCenter.cpp`

`RedisDataCenter` 会对每个 `RedisConnectionConfig` 创建一个 `RedisConnectionWorker`。

每个 worker 负责：

- 连接对应 Redis host / port / db
- 定时轮询自己负责的 key
- 接收自己负责的订阅消息

也就是说，现在不是“一个全局 worker 管全部 Redis 数据”，而是：

> 一个连接配置对应一个 worker。

### 第 3 步：worker 用 MGET 读取一组 key

轮询 key 的集合来自：

- `RedisConnectionConfig::pollingKeyGroups`

配置示意是：

```json
{
  "connectionId": "conn_main",
  "host": "127.0.0.1",
  "port": 6379,
  "db": 0,
  "pollIntervalMs": 16,
  "pollingKeyGroups": [
    { "module": "params", "keys": ["state.params.latest"] },
    { "module": "planning", "keys": ["state.planning.latest"] }
  ]
}
```

这里要注意：

- 配置里按模块分组写 key
- worker 实际读取时仍然是批量 MGET
- 模块归属信息在配置里已经有了

### 第 4 步：RedisDataCenter 做 value 归一化并分发

位置：

- `src/communication/redis/RedisDataCenter.cpp`

`RedisDataCenter::onPollBatch(connectionId, rawValues)` 当前会：

1. 找到这个 `connectionId` 对应的配置
2. 遍历本轮 MGET 返回的每个 key/value
3. 调 `normalizeValue()` 做 JSON 解码
4. 调 `cfg->moduleForKey(key)` 找到这个 key 属于哪个模块
5. 调 `LogicRuntime::onModulePollKey(module, key, value)`

也就是说，当前 polling 链路已经不是“先组一条 batch sample，再交给 parser 拆”，而是：

> `RedisDataCenter` 直接按 key 把数据分发给模块。

### 第 5 步：LogicRuntime 再转给对应模块 handler

位置：

- `src/logic/runtime/LogicRuntime.cpp`

`LogicRuntime::onModulePollKey()` 当前逻辑很简单：

- 如果 module 是 `global`，广播给所有已注册模块
- 否则找对应模块 handler
- 调 `handler->handlePollData(key, value)`

runtime 这里不做业务解析，只做模块分发。

### 第 6 步：模块收到 polling 数据

位置：

- `src/logic/registry/ModuleLogicHandler.cpp`

默认情况下，大部分模块并没有直接重写 `handlePollData()`。

框架的默认兼容行为是：

```cpp
data = {
    "key": redisKey,
    "value": normalizedValue
}
```

然后框架自动包装成一个 `StateSample`，再回调：

- `handleStateSample(sample)`

所以对模块开发者来说，当前最常见的处理入口仍然是：

- `handleStateSample(const StateSample& sample)`

这也是为什么现有模块基本还都能继续工作。

---

## 4. 当前 subscription 数据是怎么进模块的

订阅链路和 polling 很像，但更简单。

### 第 1 步：在配置文件里声明订阅频道

位置：

- `src/app/resources/redis_dispatch_config.json`

例如：

```json
"subscriptionChannels": [
  { "channel": "state.navigation", "module": "navigation" }
]
```

### 第 2 步：worker 收到 pub/sub 消息

收到消息后会把消息上抛给：

- `RedisDataCenter::onSubscription(connectionId, module, channel, payload)`

### 第 3 步：RedisDataCenter 直接调用 runtime

位置：

- `src/communication/redis/RedisDataCenter.cpp`

当前订阅路径不会经过 parser，也不会先打包成批量 sample，而是直接调用：

- `LogicRuntime::onModuleSubscription(module, channel, payload)`

### 第 4 步：runtime 转给模块

位置：

- `src/logic/runtime/LogicRuntime.cpp`

`LogicRuntime::onModuleSubscription()` 做的事是：

- 如果 module 是 `global`，广播给所有模块
- 否则找到对应模块 handler
- 调 `handler->handleSubscription(channel, payload)`

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