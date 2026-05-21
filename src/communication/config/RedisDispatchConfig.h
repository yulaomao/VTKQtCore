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
//
// Module ownership is declared inline, co-located with the key declarations
// inside pollingKeyGroups.  The special sentinel module name "global" marks
// keys that are not owned by any single module; the dispatcher handles these
// using globalDispatchRules (see below).
class RedisDispatchConfig
{
public:
    // Sentinel module name used to mark keys/channels that require dispatcher-
    // level processing instead of direct forwarding to a single module.
    static constexpr const char* kGlobalModule = "global";

    // A subscription channel together with the module that should receive it.
    // Use module == kGlobalModule to defer routing to the dispatcher.
    struct SubscriptionChannelEntry {
        QString channel;
        QString module;
    };

    // A group of polling keys that all belong to the same module, declared
    // directly inside the connection entry that fetches them.
    // Use module == kGlobalModule to defer routing to the dispatcher.
    struct PollingKeyGroup {
        QString     module; // owning module, or kGlobalModule
        QStringList keys;
    };

    // Describes how a "global" key (or prefix) should be routed by the
    // dispatcher.  The dispatcher may combine global-bucket data with other
    // module data before emitting a sample.
    //
    // keyPattern:  exact key or prefix ending with '*'
    // targetModule: the module that should receive the resolved data
    // groupName:    optional sub-map name inside the target module's payload
    //               (empty → keys are placed at the top level)
    struct GlobalDispatchRule {
        QString keyPattern;
        QString targetModule;
        QString groupName;
    };

    // Describes one physical Redis connection and the data it is responsible for.
    struct ConnectionEntry {
        QString connectionId;
        QString host;
        int port = 6379;
        int db = 0;
        int pollIntervalMs = 16;   // ~60 Hz default
        QVector<PollingKeyGroup> pollingKeyGroups;
        QVector<SubscriptionChannelEntry> subscriptionChannels;
    };

    // Describes one business module and how its data is organised.
    struct ModuleEntry {
        QString moduleId;
        QString defaultConnectionId; // which connection the module should use by default

        // Optional nesting hints: maps a group name to a list of key patterns.
        // Patterns ending with '*' are treated as prefix wildcards.
        // Keys that match a pattern are placed inside a sub-map named after the
        // group rather than at the top level of the module payload.
        // Example:
        //   { "state": ["state.navigation.latest"] }
        QMap<QString, QStringList> nestedMapStructure;
    };

    QVector<ConnectionEntry>     connections;
    QVector<ModuleEntry>         modules;

    // Dispatcher-level rules applied when a key's group module is "global".
    // Rules are evaluated in order; the first match wins.
    QVector<GlobalDispatchRule>  globalDispatchRules;

    // Returns true when at least one connection has been configured.
    bool isValid() const;

    // Pointer-stable lookup helpers (return nullptr when not found).
    const ConnectionEntry* getConnectionById(const QString& connectionId) const;
    const ModuleEntry*     getModuleById(const QString& moduleId) const;

    // Returns a flattened list of ALL polling keys declared for 'connectionId'
    // across all pollingKeyGroups (module-owned and global alike).
    QStringList pollingKeysFor(const QString& connectionId) const;

    // Returns the first GlobalDispatchRule whose keyPattern matches 'key',
    // or nullptr if no rule matches.
    const GlobalDispatchRule* findGlobalDispatchRule(const QString& key) const;

    // Returns the moduleId that owns messages arriving on 'channel' from
    // 'connectionId', or an empty string if no matching rule exists.
    QString findModuleForChannel(const QString& connectionId, const QString& channel) const;
};
