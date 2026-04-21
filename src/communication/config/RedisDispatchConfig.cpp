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

QString RedisDispatchConfig::findModuleForKey(const QString& connectionId,
                                              const QString& key) const
{
    for (const ModuleEntry& module : modules) {
        for (const KeyOwnershipEntry& ownership : module.pollingKeyOwnership) {
            if (ownership.connectionId != connectionId) {
                continue;
            }
            // Exact match first, then prefix match.
            if (key == ownership.keyPrefix || key.startsWith(ownership.keyPrefix)) {
                return module.moduleId;
            }
        }
    }
    return QString();
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
