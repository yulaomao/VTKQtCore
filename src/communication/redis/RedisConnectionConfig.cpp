#include "RedisConnectionConfig.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

// ---------------------------------------------------------------------------
// RedisConnectionConfig helpers
// ---------------------------------------------------------------------------

QStringList RedisConnectionConfig::allPollingKeys() const
{
    QStringList keys;
    for (const RedisKeyGroup& group : pollingKeyGroups) {
        for (const QString& key : group.keys) {
            if (!key.isEmpty()) {
                keys.append(key);
            }
        }
    }
    return keys;
}

QString RedisConnectionConfig::moduleForKey(const QString& key) const
{
    for (const RedisKeyGroup& group : pollingKeyGroups) {
        for (const QString& configKey : group.keys) {
            if (configKey == key) {
                return group.module;
            }
        }
    }
    return QString();
}

QString RedisConnectionConfig::moduleForChannel(const QString& channel) const
{
    for (const RedisSubChannel& sub : subscriptionChannels) {
        if (sub.channel == channel) {
            return sub.module;
        }
    }
    return QString();
}

// ---------------------------------------------------------------------------
// JSON loading
// ---------------------------------------------------------------------------

namespace {

RedisKeyGroup parseKeyGroup(const QJsonObject& obj)
{
    RedisKeyGroup group;
    group.module = obj.value(QStringLiteral("module")).toString().trimmed();

    const QJsonArray keys = obj.value(QStringLiteral("keys")).toArray();
    for (const QJsonValue& value : keys) {
        const QString key = value.toString().trimmed();
        if (!key.isEmpty()) {
            group.keys.append(key);
        }
    }
    return group;
}

RedisSubChannel parseSubChannel(const QJsonObject& obj)
{
    RedisSubChannel sub;
    sub.channel = obj.value(QStringLiteral("channel")).toString().trimmed();
    sub.module  = obj.value(QStringLiteral("module")).toString().trimmed();
    return sub;
}

RedisConnectionConfig parseConnection(const QJsonObject& obj)
{
    RedisConnectionConfig cfg;
    cfg.connectionId    = obj.value(QStringLiteral("connectionId")).toString().trimmed();
    cfg.host            = obj.value(QStringLiteral("host")).toString(QStringLiteral("127.0.0.1"));
    cfg.port            = obj.value(QStringLiteral("port")).toInt(6379);
    cfg.db              = obj.value(QStringLiteral("db")).toInt(0);
    cfg.pollIntervalMs  = obj.value(QStringLiteral("pollIntervalMs")).toInt(16);

    const QJsonArray groups = obj.value(QStringLiteral("pollingKeyGroups")).toArray();
    for (const QJsonValue& gv : groups) {
        if (!gv.isObject()) {
            continue;
        }
        const RedisKeyGroup group = parseKeyGroup(gv.toObject());
        if (!group.module.isEmpty() && !group.keys.isEmpty()) {
            cfg.pollingKeyGroups.append(group);
        }
    }

    const QJsonArray channels = obj.value(QStringLiteral("subscriptionChannels")).toArray();
    for (const QJsonValue& cv : channels) {
        if (!cv.isObject()) {
            continue;
        }
        const RedisSubChannel sub = parseSubChannel(cv.toObject());
        if (!sub.channel.isEmpty()) {
            cfg.subscriptionChannels.append(sub);
        }
    }

    return cfg;
}

} // namespace

QVector<RedisConnectionConfig>
RedisConnectionConfig::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning().noquote()
            << QStringLiteral("[RedisConnectionConfig] Cannot open file: %1").arg(filePath);
        return {};
    }
    return loadFromJson(file.readAll());
}

QVector<RedisConnectionConfig>
RedisConnectionConfig::loadFromJson(const QByteArray& json)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning().noquote()
            << QStringLiteral("[RedisConnectionConfig] JSON parse error: %1")
                   .arg(err.errorString());
        return {};
    }

    if (!doc.isObject()) {
        qWarning().noquote()
            << QStringLiteral("[RedisConnectionConfig] Root element must be a JSON object");
        return {};
    }

    const QJsonObject root = doc.object();
    QVector<RedisConnectionConfig> configs;

    const QJsonArray connections = root.value(QStringLiteral("connections")).toArray();
    for (const QJsonValue& cv : connections) {
        if (!cv.isObject()) {
            continue;
        }
        RedisConnectionConfig cfg = parseConnection(cv.toObject());
        if (cfg.connectionId.isEmpty()) {
            qWarning().noquote()
                << QStringLiteral("[RedisConnectionConfig] Skipping connection with empty connectionId");
            continue;
        }
        configs.append(cfg);
    }

    if (configs.isEmpty()) {
        qWarning().noquote()
            << QStringLiteral("[RedisConnectionConfig] No connections found in config");
    }

    return configs;
}
