# 全局 MGET 轮询分发说明

这份文档专门说明当前框架里这条链路：

`全局 MGET -> 批量 StateSample -> parser 拆分 -> LogicRuntime 分发 -> 各模块 handleStateSample()`

目标是把下面几个问题一次说清：

1. MGET 的数据是怎么从 Redis 读出来的
2. 为什么不是 polling 层直接按模块分发
3. 为什么 parser 是最适合做“手动拆分”的地方
4. 一个模块一次收到复杂嵌套结构时应该怎么组织数据
5. 轮询、订阅、模块内直接读写 Redis 之间的边界分别是什么

---

## 1. 先看整体设计

当前推荐设计是：

- **通信层只负责采集**
- **parser 负责把批量数据拆成模块样本**
- **LogicRuntime 只负责按 module 分发**
- **模块 logic 只负责处理自己的 sample**

也就是说：

### 通信层不关心业务模块

通信层只知道：

- 有一组 key 要轮询
- 要用 MGET 一次性取回来
- 返回的是一个 `QVariantMap values`

它不应该知道：

- `plane.*` 属于 `planning`
- `nav.*` 属于 `navigation`
- `params.*` 属于 `params`

这些规则都应该放在 parser。

### Runtime 不关心 key 规则

`LogicRuntime` 最好只做一件事：

- 收到 sample
- 看 `sample.module`
- 找对应 handler
- 调 `handler->handleStateSample(sample)`

Runtime 不应该写大量：

- `if key startsWith("plane.")`
- `if payload contains vertices`
- `if redisKey == xxx`

这些规则属于 parser，不属于 runtime。

---

## 2. 当前完整调用链

下面按照实际执行顺序说明。

### 第 1 步：软件初始化器注册全局 polling plan

位置：

- `src/app/software/ConcreteSoftwareInitializers/DefaultSoftwareInitializer.cpp`

初始化器在启动时，把一组要轮询的 key 注册到 `CommunicationHub`。

当前思路不再是：

- 每个模块一个 `PollingTask`

而是：

- 整个框架一个 `GlobalPollingPlan`

例如：

```cpp
GlobalPollingPlan plan(
    QStringLiteral("framework_global_poll"),
    {
        QStringLiteral("state.params.latest"),
        QStringLiteral("state.pointpick.latest"),
        QStringLiteral("state.planning.latest"),
        QStringLiteral("state.navigation.latest"),
        QStringLiteral("plane.vertices"),
        QStringLiteral("plane.triangles"),
        QStringLiteral("plane.path"),
        QStringLiteral("plane.status")
    },
    16);

plan.setChangeDetection(true);
plan.setMaxDispatchRateHz(60.0);
commHub->setGlobalPollingPlan(plan);
```

这一步只说明“要轮询哪些 key”，不说明它们属于哪个模块。

---

### 第 2 步：PollingSource 定时触发批量读取

位置：

- `src/communication/datasource/PollingSource.h`
- `src/communication/datasource/PollingSource.cpp`

`PollingSource` 负责：

- 启动定时器
- 到点后发出 `batchPollRequested(QStringList keys)`
- 接收 `RedisPollingWorker` 返回的批量结果
- 封装成一条批量 `StateSample`

这里生成的 sample 不是模块级 sample，而是一条全局 sample：

```cpp
StateSample::create(
    sourceId,
    QString(),
    QStringLiteral("global_poll_batch"),
    {
        {QStringLiteral("planId"), ...},
        {QStringLiteral("keys"), ...},
        {QStringLiteral("values"), values}
    });
```

注意这里：

- `sample.module` 是空的
- `sample.sampleType == "global_poll_batch"`

这是故意的，因为它还没有被 parser 拆分。

---

### 第 3 步：RedisPollingWorker 用 MGET 读 Redis

位置：

- `src/communication/redis/RedisPollingWorker.h`
- `src/communication/redis/RedisPollingWorker.cpp`

`RedisPollingWorker::readKeys(QStringList keys)` 做的事是：

- 把所有 key 拼成一条 `MGET`
- 调 hiredis 发送请求
- 把 Redis reply 转成 `QVariantMap`

例如：

```cpp
{
    "plane.vertices": [...],
    "plane.triangles": [...],
    "plane.path": [...],
    "plane.status": "ready",
    "state.navigation.latest": {...}
}
```

这一步仍然没有模块概念，只有 key 和 value。

---

### 第 4 步：CommunicationHub 转发样本

位置：

- `src/communication/hub/CommunicationHub.cpp`

`CommunicationHub` 只是把 `PollingSource::sampleReady` 转发成：

- `CommunicationHub::stateSampleReceived(const StateSample& sample)`

它不解析，也不决定目标模块。

---

### 第 5 步：BaseSoftwareInitializer 把 sample 送进 LogicRuntime

位置：

- `src/app/software/BaseSoftwareInitializer.cpp`

连接关系是：

```cpp
QObject::connect(commHub, &CommunicationHub::stateSampleReceived,
                 logicRuntime, &LogicRuntime::onStateSampleReceived);
```

这意味着无论 sample 来自：

- global polling
- subscription

最终都会进入：

- `LogicRuntime::onStateSampleReceived(const StateSample& sample)`

---

### 第 6 步：LogicRuntime 识别全局 polling 批量包

位置：

- `src/logic/runtime/LogicRuntime.cpp`

Runtime 首先判断：

```cpp
if (isGlobalPollingBatchSample(sample)) {
    ...
}
```

如果是全局批量包：

- 调 `m_globalPollingSampleParser->parse(sample)`
- 得到 `QVector<StateSample>`
- 对每条拆出来的 sample 再递归调用 `onStateSampleReceived(routedSample)`

也就是说，批量数据的业务拆分，发生在 parser，不发生在 runtime。

---

### 第 7 步：parser 把一包数据拆成多个模块 sample

位置：

- `src/logic/runtime/GlobalPollingSampleParser.h`
- `src/app/software/DefaultGlobalPollingSampleParser.cpp`

这是整个设计里最关键的一步。

parser 输入：

- 一条 `global_poll_batch`
- 里面包含 `values: QVariantMap`

parser 输出：

- 多条普通 `StateSample`
- 每条 sample 都必须带 `sample.module`

例如：

- 一条给 `planning`
- 一条给 `navigation`
- 一条给 `params`

这是“手动拆成几部分给不同模块”的标准做法。

---

### 第 8 步：Runtime 只按 module 分发

位置：

- `src/logic/runtime/LogicRuntime.cpp`

当 sample 不是 `global_poll_batch` 时，runtime 会做：

```cpp
QString targetModule = sample.module;
ModuleLogicHandler* handler = m_moduleLogicRegistry->getHandler(targetModule);
handler->handleStateSample(sample);
```

这就是最终调用模块 `handleStateSample` 的地方。

例如：

- `sample.module == "planning"`

则最终调用：

- `PlanningModuleLogicHandler::handleStateSample(sample)`

---

## 3. 为什么 parser 是最适合做“手动拆分”的地方

如果 MGET 回来的是一整包混合数据，例如：

```cpp
{
    "plane.vertices": [...],
    "plane.triangles": [...],
    "plane.path": [...],
    "plane.status": "ready",
    "nav.pose": {...},
    "nav.status": "tracking",
    "params.profile": {...}
}
```

那么你要做的不是让 runtime 逐 key 去猜，而是让 parser：

1. 先把 `plane.*` 收集起来
2. 再把 `nav.*` 收集起来
3. 再把 `params.*` 收集起来
4. 为每个模块构造一条 sample

也就是：

```cpp
plane.*   -> planning sample
nav.*     -> navigation sample
params.*  -> params sample
```

这样 runtime 会非常简单，模块也更稳定。

---

## 4. 推荐的 parser 写法：先分桶，再产出 sample

当前 `DefaultGlobalPollingSampleParser::parse()` 还是逐 key 直接 append sample，这适合简单演示，但不适合复杂拆分。

推荐改成两阶段：

### 第一阶段：分桶

```cpp
QVariantMap planningPayload;
QVariantMap navigationPayload;
QVariantMap paramsPayload;

for (auto it = rawValues.cbegin(); it != rawValues.cend(); ++it) {
    const QString key = it.key();
    const QVariant value = normalizeRedisValue(it.value());

    if (key.startsWith(QStringLiteral("plane."))) {
        planningPayload.insert(key.mid(QStringLiteral("plane.").size()), value);
        continue;
    }

    if (key.startsWith(QStringLiteral("nav."))) {
        navigationPayload.insert(key.mid(QStringLiteral("nav.").size()), value);
        continue;
    }

    if (key.startsWith(QStringLiteral("params."))) {
        paramsPayload.insert(key.mid(QStringLiteral("params.").size()), value);
        continue;
    }
}
```

### 第二阶段：为每个模块构造 sample

```cpp
QVector<StateSample> samples;

if (!planningPayload.isEmpty()) {
    StateSample sample = StateSample::create(
        batchSample.sourceId,
        QStringLiteral("planning"),
        QStringLiteral("planning_batch"),
        createPlanningSampleData(planningPayload, batchSample.sampleId));
    sample.timestampMs = batchSample.timestampMs;
    samples.append(sample);
}

if (!navigationPayload.isEmpty()) {
    StateSample sample = StateSample::create(
        batchSample.sourceId,
        QStringLiteral("navigation"),
        QStringLiteral("navigation_batch"),
        createNavigationSampleData(navigationPayload, batchSample.sampleId));
    sample.timestampMs = batchSample.timestampMs;
    samples.append(sample);
}

if (!paramsPayload.isEmpty()) {
    StateSample sample = StateSample::create(
        batchSample.sourceId,
        QStringLiteral("params"),
        QStringLiteral("params_batch"),
        createParamsSampleData(paramsPayload, batchSample.sampleId));
    sample.timestampMs = batchSample.timestampMs;
    samples.append(sample);
}

return samples;
```

这就是“手动拆成几部分给不同模块”的标准模板。

---

## 5. 一个模块一次收到复杂嵌套结构，为什么比平铺更合适

如果一个模块一次收到很多类数据，不建议全部平铺到顶层。

不推荐：

```cpp
{
    "vertices": [...],
    "triangles": [...],
    "path": [...],
    "status": "ready",
    "accepted": false,
    "revision": 12,
    "planId": "plan_001"
}
```

推荐：

```cpp
{
    "sourceBatchSampleId": "...",
    "group": "plane",
    "value": {
        "mesh": {
            "vertices": [...],
            "triangles": [...]
        },
        "path": {
            "points": [...]
        },
        "status": {
            "state": "ready",
            "accepted": false
        },
        "meta": {
            "planId": "plan_001",
            "revision": 12
        }
    }
}
```

这里仍然是 `QVariantMap`，只是 payload 是嵌套 map。

在 Qt 框架里：

- `QVariantMap` 本质上已经是“字典”
- 不需要换成别的容器
- 只需要把业务内容组织成嵌套字典即可

---

## 6. `createSampleData()` 该怎么理解

当前位置：

- `src/app/software/DefaultGlobalPollingSampleParser.cpp`

当前版本的 `createSampleData()` 更像是：

- “单个 key -> 单条 sample”的简单包装器

例如：

```cpp
{
    "key": redisKey,
    "value": normalizedValue,
    "sourceBatchSampleId": sourceBatchSampleId
}
```

如果你已经决定：

- 一个模块一次收到复杂结构
- 一条 sample 对应一整个聚合后的模块 payload

那么更推荐把它改成“按模块专用构造器”：

例如：

- `createPlanningSampleData(...)`
- `createNavigationSampleData(...)`
- `createParamsSampleData(...)`

这样比统一一个 `createSampleData(targetModule, ...)` 更清楚。

---

## 7. planning 示例：把 `plane.*` 聚合后交给 `PlanningModuleLogicHandler`

假设 Redis 里有这些 key：

```text
plane.vertices
plane.triangles
plane.path
plane.status
plane.accepted
```

### parser 应该做什么

先聚合成：

```cpp
QVariantMap planningPayload;
planningPayload.insert(QStringLiteral("vertices"), ...);
planningPayload.insert(QStringLiteral("triangles"), ...);
planningPayload.insert(QStringLiteral("path"), ...);
planningPayload.insert(QStringLiteral("status"), ...);
planningPayload.insert(QStringLiteral("accepted"), ...);
```

然后生成一条：

```cpp
StateSample::create(
    batchSample.sourceId,
    QStringLiteral("planning"),
    QStringLiteral("planning_batch"),
    {
        {QStringLiteral("value"), planningPayload},
        {QStringLiteral("sourceBatchSampleId"), batchSample.sampleId}
    });
```

### `PlanningModuleLogicHandler` 现在怎么接

如果 `PlanningModuleLogicHandler::handleStateSample()` 继续走：

```cpp
QVariantMap payload = sample.data.value(QStringLiteral("value")).toMap();
```

那只要你给它的 `value` 里继续保留：

- `vertices`
- `triangles`
- `path`
- `status`
- `accepted`

它基本就能继续工作。

也就是说：

- 如果想最小改动模块，就让 parser 输出兼容现有 handler 的结构
- 如果想更强 schema，就改 handler 去吃嵌套结构

---

## 8. 轮询、订阅、模块内直接 Redis 访问三者的边界

### 8.1 轮询

轮询是：

- CommunicationHub / PollingSource / RedisPollingWorker
- 周期性 MGET
- parser 拆成模块 sample
- Runtime 分发给模块

模块不直接参与 MGET 调度。

### 8.2 订阅

订阅仍然保留，但现在是显式接入，不再默认给每个模块自动注册。

如果某个软件要订阅：

```cpp
commHub->addSubscriptionSource(new SubscriptionSource(
    QStringLiteral("planning_subscription"),
    QStringLiteral("state.planning"),
    QStringLiteral("planning")));
```

这样订阅消息就会作为普通 `StateSample` 进入 runtime，再路由给 `planning`。

### 8.3 模块内直接 Redis 读写与发布

`ModuleLogicHandler` 提供了：

- `readRedisValue`
- `readRedisJsonValue`
- `writeRedisValue`
- `writeRedisJsonValue`
- `publishRedisMessage`
- `publishRedisJsonMessage`

这条链路独立于 polling / subscription 分发。

也就是说：

- 模块可以直接 get/set 一个 key
- 模块可以直接 publish 一个频道
- 这不依赖 parser
- 也不依赖 polling plan

---

## 9. 推荐的最终职责分层

推荐把职责固定成下面这样：

### CommunicationHub

只负责通信传输：

- polling transport
- subscription transport
- control transport

### GlobalPollingSampleParser

只负责：

- 批量 values 的业务拆分
- 多模块分桶
- 聚合 payload
- 生成带 `sample.module` 的普通 `StateSample`

### LogicRuntime

只负责：

- 调 parser
- 按 `sample.module` 找 handler
- 调 `handler->handleStateSample(sample)`

### ModuleLogicHandler

只负责：

- 处理自己的 `handleAction`
- 处理自己的 `handleStateSample`
- 自己调用 Redis `get/set/publish`

---

## 10. 一句话总结

如果 MGET 一次读回来的数据要“手动拆成几部分给不同模块”，最好的做法是：

> **在 `DefaultGlobalPollingSampleParser::parse()` 里先按模块分桶，再把每个桶组装成一条模块级 `StateSample`，最后交给 `LogicRuntime` 只按 `sample.module` 分发。**

这样最清晰、最容易维护，也不会把业务路由规则污染到 runtime。