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

    // Find the config for this connection so we can look up key→module.
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

    for (auto it = rawValues.cbegin(); it != rawValues.cend(); ++it) {
        const QString&  key   = it.key();
        const QVariant  value = normalizeValue(it.value());

        const QString module = cfg->moduleForKey(key);
        if (module.isEmpty()) {
            // Key not in any group — skip silently.
            continue;
        }

        moduleBatches[module].insert(key, value);
    }

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
QVariant RedisDataCenter::normalizeValue(const QVariant& raw)
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
    default:
        return raw;
    }
}
