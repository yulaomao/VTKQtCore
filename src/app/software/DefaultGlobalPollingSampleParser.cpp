#include "DefaultGlobalPollingSampleParser.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMetaType>

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

QVariant normalizeRedisValue(const QVariant& value)
{
    switch (value.userType()) {
    case QMetaType::QByteArray: {
        const QByteArray bytes = value.toByteArray();
        const QVariant decoded = decodeJsonVariant(bytes);
        return decoded.isValid() ? decoded : QVariant(QString::fromUtf8(bytes));
    }
    case QMetaType::QString: {
        const QString text = value.toString();
        const QVariant decoded = decodeJsonVariant(text.toUtf8());
        return decoded.isValid() ? decoded : QVariant(text);
    }
    default:
        return value;
    }
}

} // namespace

DefaultGlobalPollingSampleParser::DefaultGlobalPollingSampleParser(QObject* parent)
    : GlobalPollingSampleParser(parent)
{
}

void DefaultGlobalPollingSampleParser::setKeyRoutes(const QMap<QString, PollingKeyRoute>& routes)
{
    m_keyRoutes = routes;
}

QVector<StateSample> DefaultGlobalPollingSampleParser::parse(const StateSample& batchSample) const
{
    const QVariantMap rawValues = batchSample.data.value(QStringLiteral("values")).toMap();
    if (rawValues.isEmpty()) {
        return {};
    }

    // Phase 1: bucket normalised values by module using the route table.
    // moduleValues[module][subKey] = decodedValue
    QMap<QString, QVariantMap> moduleValues;

    for (auto it = rawValues.cbegin(); it != rawValues.cend(); ++it) {
        const QString& redisKey = it.key();
        const QVariant normalizedValue = normalizeRedisValue(it.value());

        const auto routeIt = m_keyRoutes.constFind(redisKey);
        if (routeIt == m_keyRoutes.constEnd()) {
            continue;
        }

        const PollingKeyRoute& route = routeIt.value();
        if (route.module.isEmpty()) {
            continue;
        }

        const QString subKey = route.subKey.isEmpty() ? redisKey : route.subKey;
        moduleValues[route.module].insert(subKey, normalizedValue);
    }

    if (moduleValues.isEmpty()) {
        return {};
    }

    // Phase 2: produce exactly one StateSample per module.
    QVector<StateSample> samples;
    samples.reserve(moduleValues.size());

    for (auto mit = moduleValues.cbegin(); mit != moduleValues.cend(); ++mit) {
        const QString& module = mit.key();
        const QVariantMap& values = mit.value();

        QVariantMap sampleData;
        sampleData.insert(QStringLiteral("values"), values);
        sampleData.insert(QStringLiteral("sourceBatchSampleId"), batchSample.sampleId);

        StateSample sample = StateSample::create(
            batchSample.sourceId,
            module,
            QStringLiteral("%1_batch").arg(module),
            sampleData);
        sample.timestampMs = batchSample.timestampMs;
        samples.append(sample);
    }

    return samples;
}
