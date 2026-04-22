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

    // Phase 1: assign every key to its owning module (bucket by module).
    QMap<QString, QVariantMap> moduleBuckets;
    for (auto it = rawValues.cbegin(); it != rawValues.cend(); ++it) {
        const QString key   = it.key();
        const QVariant value = normalizeRedisValue(it.value());

        const QString moduleId = m_config.findModuleForKey(connectionId, key);
        if (moduleId.isEmpty()) {
            qWarning().noquote()
                << QStringLiteral("[ConfigDrivenSampleParser] Key '%1' from connection '%2'"
                                  " has no configured module owner — skipped")
                       .arg(key, connectionId);
            continue;
        }

        moduleBuckets[moduleId].insert(key, value);
    }

    // Phase 2: build one StateSample per module bucket.
    QVector<StateSample> samples;
    samples.reserve(moduleBuckets.size());

    for (auto it = moduleBuckets.cbegin(); it != moduleBuckets.cend(); ++it) {
        const QString   moduleId   = it.key();
        const QVariantMap flatKeys = it.value();

        const QVariantMap nestedPayload = buildNestedPayload(moduleId, flatKeys);

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
    const QString moduleId = m_config.findModuleForChannel(connectionId, channel);
    if (moduleId.isEmpty()) {
        qWarning().noquote()
            << QStringLiteral("[ConfigDrivenSampleParser] Subscription channel '%1' from"
                              " connection '%2' has no configured module owner — skipped")
                   .arg(channel, connectionId);
        // Return a sample without a module — callers should check sample.module.isEmpty().
        return StateSample{};
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

QVariantMap ConfigDrivenSampleParser::buildNestedPayload(const QString& moduleId,
                                                          const QVariantMap& flatKeys) const
{
    const RedisDispatchConfig::ModuleEntry* entry = m_config.getModuleById(moduleId);
    if (!entry || entry->nestedMapStructure.isEmpty()) {
        // No grouping rules — return the flat map as-is.
        return flatKeys;
    }

    QVariantMap result;
    QSet<QString> assigned;

    // Place keys that match a pattern into the named group sub-map.
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

    // Keys that were not claimed by any group go at the top level.
    for (auto keyIt = flatKeys.cbegin(); keyIt != flatKeys.cend(); ++keyIt) {
        if (!assigned.contains(keyIt.key())) {
            result.insert(keyIt.key(), keyIt.value());
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
