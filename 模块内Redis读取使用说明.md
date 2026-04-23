# 模块内 Redis 读取使用说明

本文只说明模块逻辑内部如何主动读取 Redis，不涉及 UI 直接访问 Redis。

## 1. 适用范围

模块内主动读取只允许在以下位置使用：

- `ModuleLogicHandler` 子类
- `LogicRuntime`

不要在 QWidget、页面类或其他 UI 代码里直接访问 Redis。

## 2. 当前可用读取与写入接口

框架通过注入的 `IRedisCommandAccess` 暴露主动访问能力，模块层已经封装成以下保护方法。

### 2.1 普通 key 读写

- `readRedisValue(key)`
- `readRedisStringValue(key)`
- `readRedisJsonValue(key)`
- `writeRedisValue(key, value)`
- `writeRedisJsonValue(key, value)`

对应 Redis 命令：

- `GET key`
- `SET key value`

### 2.2 Hash 层级读写

新增 path 形式接口：

- `readRedisHashValue(path)`
- `readRedisHashStringValue(path)`
- `readRedisHashJsonValue(path)`
- `writeRedisHashValue(path, value)`
- `writeRedisHashJsonValue(path, value)`

其中 `path` 类型是 `QStringList`。

### 2.3 Path 语义

- `path.size() == 1`：只传顶级 hash key，读取或替换整个 hash
- `path.size() == 2`：`path[0]` 是 hash key，`path[1]` 是 field，读取或写入单个 field
- `path.size() > 2`：`path[0]` 是 hash key，`path[1]` 是 field，`path[2...]` 是该 field 内 JSON 数据的子路径

这就是“传入准确的层级结构”；如果只传顶级，就是获取整个 hash 或改整个 hash。

## 3. 返回值约定

- `readRedisValue()` / `readRedisHashValue()` 返回 `QVariant`
- `readRedisStringValue()` / `readRedisHashStringValue()` 返回 `QString`
- `readRedisJsonValue()` / `readRedisHashJsonValue()` 会优先把 Redis 中的 JSON 字符串解析成结构化 `QVariant`
- 顶级 hash 读取通常返回 `QVariantMap(field -> value)`
- 如果 key、field 或子路径不存在，通常返回空 `QVariant`、空字符串或空 `QVariantMap`

## 4. 模块内使用示例

### 4.1 读取普通 key JSON

```cpp
void ParamsModuleLogicHandler::onModuleActivated()
{
    const QVariantMap snapshot = readRedisJsonValue(QStringLiteral("state.params.latest"));
    if (snapshot.isEmpty()) {
        return;
    }

    applySnapshot(snapshot);
}
```

### 4.2 读取整个 hash

```cpp
void NavigationModuleLogicHandler::onModuleActivated()
{
  const QVariantMap transformMap = readRedisHashJsonValue(
    QStringList{QStringLiteral("demo:navigation:transform")});
  if (transformMap.isEmpty()) {
        return;
    }

  applyTransformMap(transformMap);
}
```

### 4.3 读取 hash 单字段 JSON

```cpp
const QVariantMap worldTransform = readRedisHashJsonValue(
  QStringList{
    QStringLiteral("demo:navigation:transform"),
    QStringLiteral("world")
  });
```

### 4.4 读取字段内嵌套子路径

```cpp
const QString status = readRedisHashStringValue(
  QStringList{
    QStringLiteral("state.navigation"),
    QStringLiteral("latest"),
    QStringLiteral("status")
  });
```

### 4.5 写入整个 hash

```cpp
writeRedisHashJsonValue(
  QStringList{QStringLiteral("state.params")},
  {
    {QStringLiteral("latest"), m_parameters},
    {QStringLiteral("backup"), m_backupParameters}
  });
```

### 4.6 写入字段内嵌套子路径

```cpp
writeRedisHashValue(
  QStringList{
    QStringLiteral("state.navigation"),
    QStringLiteral("latest"),
    QStringLiteral("status")
  },
  QStringLiteral("ready"));
```

## 5. 轮询配置与模块读取的关系

Redis 轮询配置格式不变，仍然写原来的总 key：

```json
{
  "module": "navigation",
  "keys": [
  "demo:navigation:transform:world",
  "demo:navigation:transform:reference"
  ]
}
```

说明：

- polling worker 内部会把总 key 解析成 `hashKey + field` 后执行 `HGET`
- 返回给模块的批量结果键名仍然是原来的总 key
- 模块处理 polling 批量数据时，仍然从 `sample.data["values"]` 里按总 key 读取

## 6. 什么时候用主动读取，什么时候用轮询

适合主动读取：

- 模块激活时补拉一次最小状态
- 用户点击按钮后立即读取确认值
- 通信恢复后做一次轻量补拉

适合轮询：

- 高频状态持续刷新
- 服务端没有主动推送，只提供 hash 或缓存字段
- 同一批数据需要持续流入模块逻辑

## 7. 约束

- UI 不允许直接读 Redis
- 不要在业务代码里自己创建 hiredis context
- 高频主动读取也必须走框架注入的 `IRedisCommandAccess`
- 订阅场景仍然走 `RedisGateway + subscription`

## 8. 推荐做法

- 模块初始化补拉用 `readRedisJsonValue()` 或 `readRedisHashJsonValue(path)`
- 高频共享状态优先放入轮询配置，而不是在模块里自己起定时器反复读
- 如果服务端数据已经改成 hash，模块侧优先使用 path 形式的 hash 读写接口
- 需要整个 hash 时只传顶级 path；需要单字段或字段内子路径时再继续追加层级