#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// ---------------------------------------------------------------------------
// Simple Redis connection configuration structs.
//
// Loaded from redis_dispatch_config.json which has this shape:
//
//   {
//     "connections": [
//       {
//         "connectionId": "conn_main",
//         "host": "127.0.0.1",
//         "port": 6379,
//         "db": 0,
//         "pollIntervalMs": 16,
//         "pollingKeyGroups": [
//           { "module": "params",    "keys": ["state.params.latest"] },
//           { "module": "pointpick", "keys": ["state.pointpick.latest"] }
//         ],
//         "subscriptionChannels": [
//           { "channel": "state.params",    "module": "params"    },
//           { "channel": "state.pointpick", "module": "pointpick" }
//         ]
//       }
//     ]
//   }
//
// module == "global" means the data is broadcast to ALL registered module
// logic handlers.  Use this for shared / cross-cutting Redis keys.
// ---------------------------------------------------------------------------

// One group of polling keys all belonging to the same module.
struct RedisKeyGroup {
    QString     module; // target module ID, or "global" to broadcast
    QStringList keys;   // Redis keys to include in MGET
};

// One Redis pub/sub channel and its owning module.
struct RedisSubChannel {
    QString channel;    // channel name to subscribe to
    QString module;     // target module ID, or "global" to broadcast
};

// Everything the worker needs for one physical Redis connection.
struct RedisConnectionConfig {
    // Module name used to broadcast data to all registered handlers.
    static constexpr const char* kGlobalModule = "global";

    QString connectionId;
    QString host            = QStringLiteral("127.0.0.1");
    int     port            = 6379;
    int     db              = 0;
    int     pollIntervalMs  = 16;   // MGET interval (≈60 Hz)

    QVector<RedisKeyGroup>   pollingKeyGroups;
    QVector<RedisSubChannel> subscriptionChannels;

    // Returns all polling keys across all groups (for MGET).
    QStringList allPollingKeys() const;

    // Returns the module that owns 'key' (exact or prefix match).
    // Returns an empty string if no group claims the key.
    QString moduleForKey(const QString& key) const;

    // Returns the module that owns 'channel'.
    // Returns an empty string if no subscription entry matches.
    QString moduleForChannel(const QString& channel) const;

    // -----------------------------------------------------------------
    // Factory helpers
    // -----------------------------------------------------------------

    // Load all connections from a JSON file (Qt resource paths ":/…" work).
    static QVector<RedisConnectionConfig> loadFromFile(const QString& filePath);

    // Load from an already-read JSON byte array.
    static QVector<RedisConnectionConfig> loadFromJson(const QByteArray& json);
};
