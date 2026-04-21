#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

// Value-object hierarchy that describes how multiple Redis connections map
// to polling keys, subscription channels, and business modules.
//
// One connection == one fixed DB.  The config is loaded once at startup by
// RedisDispatchConfigLoader and is immutable afterwards.
class RedisDispatchConfig
{
public:
    // A subscription channel together with the module that should receive it.
    struct SubscriptionChannelEntry {
        QString channel;
        QString module;
    };

    // Declares that a set of keys (identified by a common prefix) that live on
    // a particular connection belong to a specific module.
    struct KeyOwnershipEntry {
        QString connectionId;
        QString keyPrefix; // exact key OR prefix (prefix match used when the key starts with this)
    };

    // Describes one physical Redis connection and the data it is responsible for.
    struct ConnectionEntry {
        QString connectionId;
        QString host;
        int port = 6379;
        int db = 0;
        int pollIntervalMs = 16;   // ~60 Hz default
        QStringList pollingKeys;
        QVector<SubscriptionChannelEntry> subscriptionChannels;
    };

    // Describes one business module and how its data is sourced and organised.
    struct ModuleEntry {
        QString moduleId;
        QString defaultConnectionId; // which connection the module should use by default

        // List of (connectionId, keyPrefix) pairs that feed this module.
        QVector<KeyOwnershipEntry> pollingKeyOwnership;

        // Optional nesting hints: maps a group name to a list of key patterns.
        // Patterns ending with '*' are treated as prefix wildcards.
        // Keys that match a pattern are placed inside a sub-map named after the
        // group rather than at the top level of the module payload.
        // Example:
        //   { "transforms": ["demo:navigation:transform:*"],
        //     "state":      ["state.navigation.latest"] }
        QMap<QString, QStringList> nestedMapStructure;
    };

    QVector<ConnectionEntry> connections;
    QVector<ModuleEntry> modules;

    // Returns true when at least one connection has been configured.
    bool isValid() const;

    // Pointer-stable lookup helpers (return nullptr when not found).
    const ConnectionEntry* getConnectionById(const QString& connectionId) const;
    const ModuleEntry*     getModuleById(const QString& moduleId) const;

    // Returns the moduleId that owns 'key' arriving from 'connectionId',
    // or an empty string if no matching ownership rule exists.
    QString findModuleForKey(const QString& connectionId, const QString& key) const;

    // Returns the moduleId that owns messages arriving on 'channel' from
    // 'connectionId', or an empty string if no matching rule exists.
    QString findModuleForChannel(const QString& connectionId, const QString& channel) const;
};
