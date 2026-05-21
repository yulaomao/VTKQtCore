#include "RedisDispatchConfigLoader.h"

#include <QDebug>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

RedisDispatchConfig::SubscriptionChannelEntry
parseSubscriptionChannel(const QJsonObject& obj)
{
    RedisDispatchConfig::SubscriptionChannelEntry entry;
    entry.channel = obj.value(QStringLiteral("channel")).toString().trimmed();
    entry.module  = obj.value(QStringLiteral("module")).toString().trimmed();
    return entry;
}

RedisDispatchConfig::PollingKeyGroup parsePollingKeyGroup(const QJsonObject& obj)
{
    RedisDispatchConfig::PollingKeyGroup group;
    group.module = obj.value(QStringLiteral("module")).toString().trimmed();

    const QJsonArray keysArray = obj.value(QStringLiteral("keys")).toArray();
    for (const QJsonValue& v : keysArray) {
        const QString k = v.toString().trimmed();
        if (!k.isEmpty()) {
            group.keys.append(k);
        }
    }
    return group;
}

RedisDispatchConfig::ConnectionEntry parseConnection(const QJsonObject& obj)
{
    RedisDispatchConfig::ConnectionEntry entry;
    entry.connectionId  = obj.value(QStringLiteral("connectionId")).toString().trimmed();
    entry.host          = obj.value(QStringLiteral("host")).toString(QStringLiteral("127.0.0.1"));
    entry.port          = obj.value(QStringLiteral("port")).toInt(6379);
    entry.db            = obj.value(QStringLiteral("db")).toInt(0);
    entry.pollIntervalMs = obj.value(QStringLiteral("pollIntervalMs")).toInt(16);

    const QJsonArray groups = obj.value(QStringLiteral("pollingKeyGroups")).toArray();
    for (const QJsonValue& gv : groups) {
        if (!gv.isObject()) {
            continue;
        }
        const auto group = parsePollingKeyGroup(gv.toObject());
        if (!group.module.isEmpty() && !group.keys.isEmpty()) {
            entry.pollingKeyGroups.append(group);
        }
    }

    const QJsonArray channels = obj.value(QStringLiteral("subscriptionChannels")).toArray();
    for (const QJsonValue& ch : channels) {
        if (!ch.isObject()) {
            continue;
        }
        const auto channelEntry = parseSubscriptionChannel(ch.toObject());
        if (!channelEntry.channel.isEmpty()) {
            entry.subscriptionChannels.append(channelEntry);
        }
    }

    return entry;
}

RedisDispatchConfig::ModuleEntry parseModule(const QJsonObject& obj)
{
    RedisDispatchConfig::ModuleEntry entry;
    entry.moduleId           = obj.value(QStringLiteral("moduleId")).toString().trimmed();
    entry.defaultConnectionId = obj.value(QStringLiteral("defaultConnectionId")).toString().trimmed();

    const QJsonObject nestedMap = obj.value(QStringLiteral("nestedMapStructure")).toObject();
    for (auto it = nestedMap.constBegin(); it != nestedMap.constEnd(); ++it) {
        QStringList patterns;
        const QJsonArray arr = it.value().toArray();
        for (const QJsonValue& v : arr) {
            const QString p = v.toString().trimmed();
            if (!p.isEmpty()) {
                patterns.append(p);
            }
        }
        if (!patterns.isEmpty()) {
            entry.nestedMapStructure.insert(it.key(), patterns);
        }
    }

    return entry;
}

RedisDispatchConfig::GlobalDispatchRule parseGlobalDispatchRule(const QJsonObject& obj)
{
    RedisDispatchConfig::GlobalDispatchRule rule;
    rule.keyPattern   = obj.value(QStringLiteral("keyPattern")).toString().trimmed();
    rule.targetModule = obj.value(QStringLiteral("targetModule")).toString().trimmed();
    rule.groupName    = obj.value(QStringLiteral("groupName")).toString().trimmed();
    return rule;
}

} // namespace

RedisDispatchConfig RedisDispatchConfigLoader::loadFromFile(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning().noquote()
            << QStringLiteral("[RedisDispatchConfigLoader] Cannot open config file: %1")
                   .arg(filePath);
        return RedisDispatchConfig{};
    }

    const QByteArray data = file.readAll();
    return loadFromJson(data);
}

RedisDispatchConfig RedisDispatchConfigLoader::loadFromJson(const QByteArray& json)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        qWarning().noquote()
            << QStringLiteral("[RedisDispatchConfigLoader] JSON parse error: %1")
                   .arg(parseError.errorString());
        return RedisDispatchConfig{};
    }

    if (!doc.isObject()) {
        qWarning().noquote()
            << QStringLiteral("[RedisDispatchConfigLoader] Root element must be a JSON object");
        return RedisDispatchConfig{};
    }

    const QJsonObject root = doc.object();
    RedisDispatchConfig config;

    const QJsonArray connections = root.value(QStringLiteral("connections")).toArray();
    for (const QJsonValue& connVal : connections) {
        if (!connVal.isObject()) {
            qWarning().noquote()
                << QStringLiteral("[RedisDispatchConfigLoader] Skipping non-object in connections array");
            continue;
        }
        const auto connEntry = parseConnection(connVal.toObject());
        if (connEntry.connectionId.isEmpty()) {
            qWarning().noquote()
                << QStringLiteral("[RedisDispatchConfigLoader] Skipping connection entry with empty connectionId");
            continue;
        }
        config.connections.append(connEntry);
    }

    const QJsonArray modules = root.value(QStringLiteral("modules")).toArray();
    for (const QJsonValue& modVal : modules) {
        if (!modVal.isObject()) {
            qWarning().noquote()
                << QStringLiteral("[RedisDispatchConfigLoader] Skipping non-object in modules array");
            continue;
        }
        const auto modEntry = parseModule(modVal.toObject());
        if (modEntry.moduleId.isEmpty()) {
            qWarning().noquote()
                << QStringLiteral("[RedisDispatchConfigLoader] Skipping module entry with empty moduleId");
            continue;
        }
        if (modEntry.defaultConnectionId.isEmpty()) {
            qWarning().noquote()
                << QStringLiteral("[RedisDispatchConfigLoader] Module '%1' has no defaultConnectionId")
                       .arg(modEntry.moduleId);
        }
        config.modules.append(modEntry);
    }

    const QJsonArray rules = root.value(QStringLiteral("globalDispatchRules")).toArray();
    for (const QJsonValue& ruleVal : rules) {
        if (!ruleVal.isObject()) {
            qWarning().noquote()
                << QStringLiteral("[RedisDispatchConfigLoader] Skipping non-object in globalDispatchRules array");
            continue;
        }
        const auto rule = parseGlobalDispatchRule(ruleVal.toObject());
        if (rule.keyPattern.isEmpty() || rule.targetModule.isEmpty()) {
            qWarning().noquote()
                << QStringLiteral("[RedisDispatchConfigLoader] Skipping globalDispatchRule with"
                                  " empty keyPattern or targetModule");
            continue;
        }
        config.globalDispatchRules.append(rule);
    }

    if (!config.isValid()) {
        qWarning().noquote()
            << QStringLiteral("[RedisDispatchConfigLoader] Config loaded but contains no connections");
    }

    return config;
}
