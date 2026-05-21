#include "ConfigDrivenSampleParser.h"

#include <QByteArray>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMap>
#include <QSet>

namespace {

QVariant decodeJsonVariant(const QByteArray& payload)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || document.isNull()) {
        return QVariant();
    }
    return document.toVariant();
}

} // namespace

ConfigDrivenSampleParser::ConfigDrivenSampleParser(const RedisDispatchConfig& config,
                                                   QObject* parent)
    : GlobalPollingSampleParser(parent)
    , m_config(config)
{
}

QVector<StateSample>
ConfigDrivenSampleParser::parse(const StateSample& batchSample) const
{
    const QString connectionId =
        batchSample.data.value(QStringLiteral("connectionId")).toString();
    const QVariantMap rawValues =
        batchSample.data.value(QStringLiteral("values")).toMap();

    if (rawValues.isEmpty()) {
        return {};
    }

    // Build a fast key→module lookup from the connection's pollingKeyGroups.
    // Keys in the "global" group are tracked separately for dispatcher-level
    // routing via globalDispatchRules.
    const RedisDispatchConfig::ConnectionEntry* conn =
        m_config.getConnectionById(connectionId);

    QMap<QString, QString> keyToModule; // key → module (excludes global keys)
    QSet<QString>          globalKeys;  // keys belonging to the "global" group

    if (conn) {
        for (const RedisDispatchConfig::PollingKeyGroup& group : conn->pollingKeyGroups) {
            const bool isGlobal =
                (group.module == QLatin1String(RedisDispatchConfig::kGlobalModule));

            for (const QString& configKey : group.keys) {
                // configKey may be a prefix pattern (ends with '*') or exact key.
                // Match raw keys from the batch against each configured key/pattern.
                if (isGlobal) {
                    for (auto it = rawValues.cbegin(); it != rawValues.cend(); ++it) {
                        if (matchesPattern(it.key(), configKey)) {
                            globalKeys.insert(it.key());
                        }
                    }
                } else {
                    for (auto it = rawValues.cbegin(); it != rawValues.cend(); ++it) {
                        if (matchesPattern(it.key(), configKey) &&
                            !keyToModule.contains(it.key()))
                        {
                            keyToModule.insert(it.key(), group.module);
                        }
                    }
                }
            }
        }
    }

    // Phase 1: bucket module-owned keys by module.
    QMap<QString, QVariantMap> moduleBuckets;
    for (auto it = rawValues.cbegin(); it != rawValues.cend(); ++it) {
        const QString& key = it.key();
        if (globalKeys.contains(key)) {
            continue; // handled in Phase 2
        }

        const auto moduleIt = keyToModule.constFind(key);
        if (moduleIt == keyToModule.cend()) {
            qWarning().noquote()
                << QStringLiteral("[ConfigDrivenSampleParser] Key '%1' from connection '%2'"
                                  " has no configured module owner — skipped")
                       .arg(key, connectionId);
            continue;
        }

        moduleBuckets[moduleIt.value()].insert(key, normalizeRedisValue(it.value()));
    }

    // Phase 2: route global keys through globalDispatchRules, grouping them
    // by (targetModule, groupName) so they can be merged into the target's bucket.
    //
    // globalPreGrouped[targetModule][groupName][key] = value
    // An empty groupName means the key goes at the top level of the module payload.
    QMap<QString, QMap<QString, QVariantMap>> globalPreGrouped;

    for (const QString& key : globalKeys) {
        const QVariant value = normalizeRedisValue(rawValues.value(key));
        const RedisDispatchConfig::GlobalDispatchRule* rule =
            m_config.findGlobalDispatchRule(key);

        if (!rule) {
            qWarning().noquote()
                << QStringLiteral("[ConfigDrivenSampleParser] Global key '%1' from"
                                  " connection '%2' has no matching globalDispatchRule — skipped")
                       .arg(key, connectionId);
            continue;
        }

        globalPreGrouped[rule->targetModule][rule->groupName].insert(key, value);
    }

    // Merge the pre-grouped global data into the appropriate module buckets.
    // If a target module already has a regular-key bucket, we extend it; otherwise
    // we create a new bucket so that a sample is still emitted for global-only data.
    for (auto moduleIt = globalPreGrouped.cbegin();
         moduleIt != globalPreGrouped.cend();
         ++moduleIt)
    {
        const QString& targetModule = moduleIt.key();
        // Ensure the module bucket exists (default-constructs an empty QVariantMap if absent).
        moduleBuckets[targetModule];
        // We don't insert global keys into the flat bucket here; we pass the
        // pre-grouped map into buildNestedPayload so it can be merged correctly.
    }

    // Phase 3: build one StateSample per module bucket.
    QVector<StateSample> samples;
    samples.reserve(moduleBuckets.size());

    for (auto it = moduleBuckets.cbegin(); it != moduleBuckets.cend(); ++it) {
        const QString&    moduleId   = it.key();
        const QVariantMap flatKeys   = it.value();

        // Collect any pre-grouped global sub-maps for this module.
        const QMap<QString, QVariantMap> preGrouped =
            globalPreGrouped.value(moduleId);

        const QVariantMap nestedPayload =
            buildNestedPayload(moduleId, flatKeys, preGrouped);

        QVariantMap data;
        data.insert(QStringLiteral("value"),              nestedPayload);
        data.insert(QStringLiteral("connectionId"),       connectionId);
        data.insert(QStringLiteral("sourceBatchSampleId"), batchSample.sampleId);

        StateSample sample = StateSample::create(
            batchSample.sourceId,
            moduleId,
            QStringLiteral("module_batch"),
            data);
        sample.timestampMs = batchSample.timestampMs;
        samples.append(sample);
    }

    return samples;
}

StateSample ConfigDrivenSampleParser::parseSubscription(const QString& connectionId,
                                                         const QString& channel,
                                                         const QVariantMap& payload) const
{
    QString moduleId = m_config.findModuleForChannel(connectionId, channel);
    if (moduleId.isEmpty()) {
        qWarning().noquote()
            << QStringLiteral("[ConfigDrivenSampleParser] Subscription channel '%1' from"
                              " connection '%2' has no configured module owner — skipped")
                   .arg(channel, connectionId);
        return StateSample{};
    }

    // If the channel is tagged "global", apply globalDispatchRules using the
    // channel name as the lookup key to resolve the actual target module.
    if (moduleId == QLatin1String(RedisDispatchConfig::kGlobalModule)) {
        const RedisDispatchConfig::GlobalDispatchRule* rule =
            m_config.findGlobalDispatchRule(channel);
        if (!rule) {
            qWarning().noquote()
                << QStringLiteral("[ConfigDrivenSampleParser] Global subscription channel '%1'"
                                  " from connection '%2' has no matching globalDispatchRule — skipped")
                       .arg(channel, connectionId);
            return StateSample{};
        }
        moduleId = rule->targetModule;
    }

    QVariantMap data = payload;
    data.insert(QStringLiteral("connectionId"), connectionId);
    data.insert(QStringLiteral("channel"),      channel);

    return StateSample::create(
        connectionId,
        moduleId,
        QStringLiteral("subscription"),
        data);
}

// static
QVariant ConfigDrivenSampleParser::normalizeRedisValue(const QVariant& raw)
{
    switch (raw.userType()) {
    case QMetaType::QByteArray: {
        const QByteArray bytes = raw.toByteArray();
        const QVariant decoded = decodeJsonVariant(bytes);
        return decoded.isValid() ? decoded : QVariant(QString::fromUtf8(bytes));
    }
    case QMetaType::QString: {
        const QByteArray bytes = raw.toString().toUtf8();
        const QVariant decoded = decodeJsonVariant(bytes);
        return decoded.isValid() ? decoded : raw;
    }
    default:
        return raw;
    }
}

QVariantMap ConfigDrivenSampleParser::buildNestedPayload(
    const QString& moduleId,
    const QVariantMap& flatKeys,
    const QMap<QString, QVariantMap>& preGrouped) const
{
    const RedisDispatchConfig::ModuleEntry* entry = m_config.getModuleById(moduleId);
    const bool hasNesting = entry && !entry->nestedMapStructure.isEmpty();

    QVariantMap result;
    QSet<QString> assigned;

    if (hasNesting) {
        // Place regular keys that match a nestedMapStructure pattern into the
        // named group sub-map.
        for (auto groupIt = entry->nestedMapStructure.cbegin();
             groupIt != entry->nestedMapStructure.cend();
             ++groupIt)
        {
            const QString&     groupName = groupIt.key();
            const QStringList& patterns  = groupIt.value();

            QVariantMap groupMap;
            for (auto keyIt = flatKeys.cbegin(); keyIt != flatKeys.cend(); ++keyIt) {
                const QString& key = keyIt.key();
                if (assigned.contains(key)) {
                    continue;
                }
                for (const QString& pattern : patterns) {
                    if (matchesPattern(key, pattern)) {
                        groupMap.insert(key, keyIt.value());
                        assigned.insert(key);
                        break;
                    }
                }
            }

            if (!groupMap.isEmpty()) {
                result.insert(groupName, groupMap);
            }
        }
    }

    // Regular keys not claimed by any nestedMapStructure pattern go at the top level.
    for (auto keyIt = flatKeys.cbegin(); keyIt != flatKeys.cend(); ++keyIt) {
        if (!assigned.contains(keyIt.key())) {
            result.insert(keyIt.key(), keyIt.value());
        }
    }

    // Merge pre-grouped global data.  Each entry in preGrouped is either a
    // named sub-map (non-empty groupName) or a set of top-level keys (empty groupName).
    for (auto pgIt = preGrouped.cbegin(); pgIt != preGrouped.cend(); ++pgIt) {
        const QString&    groupName = pgIt.key();
        const QVariantMap groupData = pgIt.value();

        if (groupName.isEmpty()) {
            // Insert global keys at the top level (don't overwrite existing keys).
            for (auto kv = groupData.cbegin(); kv != groupData.cend(); ++kv) {
                if (!result.contains(kv.key())) {
                    result.insert(kv.key(), kv.value());
                }
            }
        } else {
            // Merge into or create a named sub-map.
            QVariantMap sub = result.value(groupName).toMap();
            for (auto kv = groupData.cbegin(); kv != groupData.cend(); ++kv) {
                sub.insert(kv.key(), kv.value());
            }
            result.insert(groupName, sub);
        }
    }

    return result;
}

// static
bool ConfigDrivenSampleParser::matchesPattern(const QString& key,
                                               const QString& pattern)
{
    if (pattern.endsWith(QLatin1Char('*'))) {
        const QString prefix = pattern.left(pattern.size() - 1);
        // An empty prefix (pattern == "*") matches everything.
        return prefix.isEmpty() || key.startsWith(prefix);
    }
    return key == pattern;
}
