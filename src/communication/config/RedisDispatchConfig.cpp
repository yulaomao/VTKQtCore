#include "RedisDispatchConfig.h"

bool RedisDispatchConfig::isValid() const
{
    return !connections.isEmpty();
}

const RedisDispatchConfig::ConnectionEntry*
RedisDispatchConfig::getConnectionById(const QString& connectionId) const
{
    for (const ConnectionEntry& entry : connections) {
        if (entry.connectionId == connectionId) {
            return &entry;
        }
    }
    return nullptr;
}

const RedisDispatchConfig::ModuleEntry*
RedisDispatchConfig::getModuleById(const QString& moduleId) const
{
    for (const ModuleEntry& entry : modules) {
        if (entry.moduleId == moduleId) {
            return &entry;
        }
    }
    return nullptr;
}

QStringList RedisDispatchConfig::pollingKeysFor(const QString& connectionId) const
{
    const ConnectionEntry* conn = getConnectionById(connectionId);
    if (!conn) {
        return QStringList();
    }

    QStringList keys;
    for (const PollingKeyGroup& group : conn->pollingKeyGroups) {
        for (const QString& key : group.keys) {
            if (!key.isEmpty()) {
                keys.append(key);
            }
        }
    }
    return keys;
}

const RedisDispatchConfig::GlobalDispatchRule*
RedisDispatchConfig::findGlobalDispatchRule(const QString& key) const
{
    for (const GlobalDispatchRule& rule : globalDispatchRules) {
        const QString& pattern = rule.keyPattern;
        if (pattern.endsWith(QLatin1Char('*'))) {
            const QString prefix = pattern.left(pattern.size() - 1);
            if (prefix.isEmpty() || key.startsWith(prefix)) {
                return &rule;
            }
        } else if (key == pattern) {
            return &rule;
        }
    }
    return nullptr;
}

QString RedisDispatchConfig::findModuleForChannel(const QString& connectionId,
                                                   const QString& channel) const
{
    const ConnectionEntry* conn = getConnectionById(connectionId);
    if (!conn) {
        return QString();
    }

    for (const SubscriptionChannelEntry& entry : conn->subscriptionChannels) {
        if (entry.channel == channel) {
            return entry.module;
        }
    }
    return QString();
}
