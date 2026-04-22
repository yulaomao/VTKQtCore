#pragma once

#include <QString>
#include <QStringList>
#include <QVector>

// One group of Redis keys that belong to the same logical module.
struct RedisKeyGroup {
    QString module;       // module name that handles these keys
    QStringList keys;     // Redis keys to MGET
};

// A Redis Pub/Sub channel mapped to a logical module.
struct RedisSubscriptionChannel {
    QString channel;      // Redis channel to subscribe
    QString module;       // module name that handles messages on this channel
};

// Configuration for one Redis connection that performs both MGET polling
// and Pub/Sub subscriptions.
struct RedisConnectionConfig {
    QString connectionId;
    QString host            = QStringLiteral("127.0.0.1");
    int     port            = 6379;
    int     db              = 0;          // Redis DB index (SELECT <db>)
    int     pollIntervalMs  = 16;         // MGET polling interval in milliseconds

    QVector<RedisKeyGroup>            pollingKeyGroups;
    QVector<RedisSubscriptionChannel> subscriptionChannels;
};
