#include "RedisDataCenter.h"

#include "RedisConnectionWorker.h"
#include "logic/runtime/LogicRuntime.h"

#include <QByteArray>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QMap>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

QVariant decodeJsonBytes(const QByteArray& bytes)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(bytes, &err);
    if (err.error == QJsonParseError::NoError && !doc.isNull()) {
        return doc.toVariant();
    }
    return QVariant();
}

QVariant decodeStructuredValue(const QVariant& raw)
{
    switch (raw.userType()) {
    case QMetaType::QByteArray: {
        const QByteArray bytes = raw.toByteArray();
        const QVariant decoded = decodeJsonBytes(bytes);
        return decoded.isValid() ? decoded : QVariant(QString::fromUtf8(bytes));
    }
    case QMetaType::QString: {
        const QByteArray bytes = raw.toString().toUtf8();
        const QVariant decoded = decodeJsonBytes(bytes);
        return decoded.isValid() ? decoded : raw;
    }
    case QMetaType::QVariantMap: {
        const QVariantMap map = raw.toMap();
        QVariantMap decodedMap;
        for (auto it = map.cbegin(); it != map.cend(); ++it) {
            decodedMap.insert(it.key(), decodeStructuredValue(it.value()));
        }
        return decodedMap;
    }
    case QMetaType::QVariantList: {
        const QVariantList list = raw.toList();
        QVariantList decodedList;
        decodedList.reserve(list.size());
        for (const QVariant& entry : list) {
            decodedList.append(decodeStructuredValue(entry));
        }
        return decodedList;
    }
    default:
        return raw;
    }
}

QVariant collapseSingleLatestField(const QVariant& raw)
{
    const QVariantMap map = raw.toMap();
    if (map.size() == 1 && map.contains(QStringLiteral("latest"))) {
        return map.value(QStringLiteral("latest"));
    }

    return raw;
}

struct RestoredHashPrefix {
    QString prefix;
    QChar separator;
    bool valid = false;
};

RestoredHashPrefix resolveRestoredHashPrefix(const QString& redisKey)
{
    const int dotIndex = redisKey.lastIndexOf(QLatin1Char('.'));
    const int colonIndex = redisKey.lastIndexOf(QLatin1Char(':'));
    const int splitIndex = (std::max)(dotIndex, colonIndex);
    if (splitIndex <= 0 || splitIndex >= redisKey.size() - 1) {
        return {};
    }

    RestoredHashPrefix prefix;
    prefix.prefix = redisKey.left(splitIndex);
    prefix.separator = redisKey.at(splitIndex);
    prefix.valid = true;
    return prefix;
}

QVariantMap restoreGroupedHashEntries(const QString& redisKey,
                                      const QVariantMap& hashEntries)
{
    QVariantMap restoredEntries;
    const RestoredHashPrefix restoredPrefix = resolveRestoredHashPrefix(redisKey);
    if (!restoredPrefix.valid) {
        return restoredEntries;
    }

    for (auto it = hashEntries.cbegin(); it != hashEntries.cend(); ++it) {
        restoredEntries.insert(
            QStringLiteral("%1%2%3")
                .arg(restoredPrefix.prefix, QString(restoredPrefix.separator), it.key()),
            it.value());
    }

    return restoredEntries;
}

QVariantMap restorePolledEntries(const QString& redisKey, const QVariant& normalized)
{
    const QVariant collapsed = collapseSingleLatestField(normalized);
    const QVariantMap hashEntries = collapsed.toMap();
    if (!hashEntries.isEmpty()) {
        const QVariantMap restoredEntries = restoreGroupedHashEntries(redisKey, hashEntries);
        if (!restoredEntries.isEmpty()) {
            return restoredEntries;
        }
    }

    return {{redisKey, collapsed}};
}

} // namespace

// ---------------------------------------------------------------------------
// RedisDataCenter
// ---------------------------------------------------------------------------

RedisDataCenter::RedisDataCenter(const QVector<RedisConnectionConfig>& configs,
                                  LogicRuntime* runtime,
                                  QObject* parent)
    : QObject(parent)
    , m_configs(configs)
    , m_runtime(runtime)
{
    m_workers.reserve(configs.size());

    for (const RedisConnectionConfig& cfg : configs) {
        auto* worker = new RedisConnectionWorker(cfg, this);

        connect(worker, &RedisConnectionWorker::pollBatchReady,
                this, &RedisDataCenter::onPollBatch,
                Qt::QueuedConnection);

        connect(worker, &RedisConnectionWorker::subscriptionReceived,
                this, &RedisDataCenter::onSubscription,
                Qt::QueuedConnection);

        m_workers.append(worker);
    }
}

RedisDataCenter::~RedisDataCenter()
{
    stop();
}

void RedisDataCenter::start()
{
    if (m_running) {
        return;
    }
    m_running = true;

    for (RedisConnectionWorker* worker : m_workers) {
        worker->start();
    }

    qDebug().noquote()
        << QStringLiteral("[RedisDataCenter] Started with %1 connection(s)")
               .arg(m_workers.size());
}

void RedisDataCenter::stop()
{
    if (!m_running) {
        return;
    }
    m_running = false;

    for (RedisConnectionWorker* worker : m_workers) {
        worker->stop();
    }

    qDebug().noquote() << QStringLiteral("[RedisDataCenter] Stopped");
}

bool RedisDataCenter::isRunning() const
{
    return m_running;
}

// ---------------------------------------------------------------------------
// Polling dispatch
// ---------------------------------------------------------------------------

void RedisDataCenter::onPollBatch(const QString& connectionId,
                                   const QVariantMap& rawValues)
{
    if (!m_runtime || rawValues.isEmpty()) {
        return;
    }

    // Find the config for this connection so we can look up logical key→module.
    const RedisConnectionConfig* cfg = nullptr;
    for (const RedisConnectionConfig& c : m_configs) {
        if (c.connectionId == connectionId) {
            cfg = &c;
            break;
        }
    }

    if (!cfg) {
        qWarning().noquote()
            << QStringLiteral("[RedisDataCenter] Unknown connectionId in poll batch: %1")
                   .arg(connectionId);
        return;
    }

    // Aggregate values by module, then dispatch one batch per module.
    QMap<QString, QVariantMap> moduleBatches;
    QVariantMap normalizedValues;

    for (auto it = rawValues.cbegin(); it != rawValues.cend(); ++it) {
        const QString& key = it.key();
        const QVariantMap restoredEntries = normalizeEntries(key, it.value());

        const QString module = cfg->moduleForKey(key);
        if (module.isEmpty()) {
            // Field not in any group — skip silently.
            continue;
        }

        for (auto restoredIt = restoredEntries.cbegin(); restoredIt != restoredEntries.cend(); ++restoredIt) {
            normalizedValues.insert(restoredIt.key(), restoredIt.value());
            moduleBatches[module].insert(restoredIt.key(), restoredIt.value());
        }
    }

    qDebug().noquote()
        << QStringLiteral("[RedisDataCenter] poll batch captured connection=%1 values=%2")
               .arg(connectionId,
                    QString::fromUtf8(
                        QJsonDocument::fromVariant(normalizedValues)
                            .toJson(QJsonDocument::Compact)));

    for (auto it = moduleBatches.cbegin(); it != moduleBatches.cend(); ++it) {
        m_runtime->onModulePollBatch(it.key(), it.value());
    }
}

// ---------------------------------------------------------------------------
// Subscription dispatch
// ---------------------------------------------------------------------------

void RedisDataCenter::onSubscription(const QString& connectionId,
                                      const QString& module,
                                      const QString& channel,
                                      const QVariantMap& payload)
{
    Q_UNUSED(connectionId)

    if (!m_runtime) {
        return;
    }

    m_runtime->onModuleSubscription(module, channel, payload);
}

// ---------------------------------------------------------------------------
// Value normalisation
// ---------------------------------------------------------------------------

// static
QVariantMap RedisDataCenter::normalizeEntries(const QString& redisKey, const QVariant& raw)
{
    return restorePolledEntries(redisKey, decodeStructuredValue(raw));
}
