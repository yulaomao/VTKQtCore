#pragma once

#include "logic/runtime/GlobalPollingSampleParser.h"
#include "communication/config/RedisDispatchConfig.h"

// A GlobalPollingSampleParser that derives all routing rules from a
// RedisDispatchConfig rather than hard-coding them in C++.
//
// Polling path
// ============
// Input : a "global_poll_batch" StateSample whose data map contains:
//   - "values"       (QVariantMap) – raw key→value from MGET
//   - "connectionId" (QString)     – which connection produced the batch
//
// The parser:
//   1. For every key in "values", calls config.findModuleForKey() to determine
//      the owning module.
//   2. Groups keys into per-module buckets.
//   3. For each bucket, builds a nested QVariantMap according to the module's
//      nestedMapStructure (keys matching a wildcard pattern end up in a named
//      sub-map; unmatched keys are placed at the top level).
//   4. Emits one StateSample per non-empty module bucket.
//
// Subscription path
// =================
// parseSubscription() converts a single subscription message into a
// StateSample routed to the owning module.  The result can be fed directly
// into LogicRuntime::onStateSampleReceived().
//
// Keys with no matching module rule are silently skipped (a warning is logged).
class ConfigDrivenSampleParser : public GlobalPollingSampleParser
{
public:
    explicit ConfigDrivenSampleParser(const RedisDispatchConfig& config,
                                      QObject* parent = nullptr);

    // GlobalPollingSampleParser interface.
    QVector<StateSample> parse(const StateSample& batchSample) const override;

    // Convert a single subscription message to a module-bound StateSample.
    // Returns an invalid (empty module) sample if no matching rule is found.
    StateSample parseSubscription(const QString& connectionId,
                                  const QString& channel,
                                  const QVariantMap& payload) const;

private:
    // Decode a raw Redis value (QByteArray / QString) to a proper QVariant.
    static QVariant normalizeRedisValue(const QVariant& raw);

    // Build a nested QVariantMap for 'moduleId' from a flat key→value map.
    // Keys that match a pattern in nestedMapStructure are placed under the
    // corresponding group name; remaining keys are placed at the top level.
    QVariantMap buildNestedPayload(const QString& moduleId,
                                   const QVariantMap& flatKeys) const;

    // Returns true if 'key' matches 'pattern'.
    // Patterns ending with '*' are treated as prefix wildcards.
    static bool matchesPattern(const QString& key, const QString& pattern);

    RedisDispatchConfig m_config;
};
