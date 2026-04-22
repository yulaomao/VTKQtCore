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
//   1. For every key in "values", finds its owning module by scanning the
//      connection's pollingKeyGroups.
//   2. Module-owned keys (module != "global") are placed directly into a
//      per-module bucket.
//   3. Keys belonging to the "global" group are routed through
//      globalDispatchRules: each rule specifies a targetModule and an
//      optional groupName. The resolved values are merged into the target
//      module's bucket under the named sub-map (or at the top level if
//      groupName is empty). Global data may be combined with data from other
//      module buckets before emitting the final sample.
//   4. Emits one StateSample per non-empty module bucket.
//
// Subscription path
// =================
// parseSubscription() converts a single subscription message into a
// StateSample routed to the owning module.  If the channel's module is
// "global", the dispatcher applies globalDispatchRules to find the target.
// The result can be fed directly into LogicRuntime::onStateSampleReceived().
//
// Keys with no matching group or rule are skipped (a warning is logged).
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

    // Build a nested QVariantMap for 'moduleId' from a flat key→value map,
    // additionally merging in pre-grouped global data (groupName → key→value).
    // Keys matching nestedMapStructure patterns are placed under the group
    // name; remaining keys go at the top level.
    QVariantMap buildNestedPayload(const QString& moduleId,
                                   const QVariantMap& flatKeys,
                                   const QMap<QString, QVariantMap>& preGrouped) const;

    // Returns true if 'key' matches 'pattern'.
    // Patterns ending with '*' are treated as prefix wildcards.
    static bool matchesPattern(const QString& key, const QString& pattern);

    RedisDispatchConfig m_config;
};
