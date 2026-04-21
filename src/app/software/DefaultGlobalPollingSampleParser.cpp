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

QString resolveModuleFromKey(const QString& redisKey)
{
    if (redisKey == QStringLiteral("state.params.latest")) {
        return QStringLiteral("params");
    }
    if (redisKey == QStringLiteral("state.pointpick.latest")) {
        return QStringLiteral("pointpick");
    }
    if (redisKey == QStringLiteral("state.planning.latest")) {
        return QStringLiteral("planning");
    }
    if (redisKey == QStringLiteral("state.navigation.latest") ||
        redisKey.startsWith(QStringLiteral("demo:navigation:transform:"))) {
        return QStringLiteral("navigation");
    }

    return QString();
}

QString resolveModuleFromPayload(const QVariantMap& payload)
{
    if (payload.contains(QStringLiteral("parameters")) ||
        payload.contains(QStringLiteral("parameterCount"))) {
        return QStringLiteral("params");
    }

    if (payload.contains(QStringLiteral("points")) ||
        payload.contains(QStringLiteral("confirmed")) ||
        payload.contains(QStringLiteral("selectedIndex"))) {
        return QStringLiteral("pointpick");
    }

    if (payload.contains(QStringLiteral("vertices")) ||
        payload.contains(QStringLiteral("triangles")) ||
        payload.contains(QStringLiteral("path")) ||
        payload.contains(QStringLiteral("accepted"))) {
        return QStringLiteral("planning");
    }

    if (payload.contains(QStringLiteral("matrixToParent")) ||
        payload.contains(QStringLiteral("nodeId")) ||
        payload.contains(QStringLiteral("navigating")) ||
        payload.contains(QStringLiteral("worldMatrix")) ||
        payload.contains(QStringLiteral("tipMatrix"))) {
        return QStringLiteral("navigation");
    }

    return QString();
}

QVariantMap createSampleData(const QString& targetModule,
                            const QString& redisKey,
                            const QVariant& normalizedValue,
                            const QString& sourceBatchSampleId)
{
    QVariantMap data;
    data.insert(QStringLiteral("key"), redisKey);
    data.insert(QStringLiteral("value"), normalizedValue);
    data.insert(QStringLiteral("sourceBatchSampleId"), sourceBatchSampleId);

    if (targetModule == QStringLiteral("params")) {
        const QVariantMap parameters = normalizedValue.toMap();
        if (!parameters.isEmpty()) {
            data.insert(QStringLiteral("parameters"), parameters);
        }
    }

    return data;
}

}

DefaultGlobalPollingSampleParser::DefaultGlobalPollingSampleParser(QObject* parent)
    : GlobalPollingSampleParser(parent)
{
}

QVector<StateSample> DefaultGlobalPollingSampleParser::parse(const StateSample& batchSample) const
{
    const QVariantMap rawValues = batchSample.data.value(QStringLiteral("values")).toMap();
    if (rawValues.isEmpty()) {
        return {};
    }

    QVector<StateSample> samples;
    samples.reserve(rawValues.size());

    for (auto it = rawValues.cbegin(); it != rawValues.cend(); ++it) {
        const QVariant normalizedValue = normalizeRedisValue(it.value());
        const QVariantMap normalizedMap = normalizedValue.toMap();

        QString targetModule = resolveModuleFromKey(it.key());
        if (targetModule.isEmpty() && !normalizedMap.isEmpty()) {
            targetModule = resolveModuleFromPayload(normalizedMap);
        }
        if (targetModule.isEmpty()) {
            continue;
        }

        StateSample sample = StateSample::create(
            batchSample.sourceId,
            targetModule,
            it.key(),
            createSampleData(targetModule, it.key(), normalizedValue, batchSample.sampleId));
        sample.timestampMs = batchSample.timestampMs;
        samples.append(sample);
    }

    return samples;
}