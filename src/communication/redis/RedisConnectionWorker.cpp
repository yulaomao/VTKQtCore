#include "RedisConnectionWorker.h"

#include "communication/redis/RedisGateway.h"
#include "communication/redis/RedisPollingWorker.h"

#include <QByteArray>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QThread>
#include <QTimer>

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace {

// Returns the parsed JSON object from raw bytes, or an invalid-flag map on failure.
// Distinguishes between a valid empty JSON object `{}` and a parse error.
struct ParseResult {
    QVariantMap map;
    bool        ok = false;
};

ParseResult payloadFromJsonBytes(const QByteArray& raw)
{
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(raw, &err);
    if (err.error == QJsonParseError::NoError && doc.isObject()) {
        return {doc.object().toVariantMap(), true};
    }
    return {{}, false};
}

} // namespace

// ---------------------------------------------------------------------------
// RedisConnectionWorker
// ---------------------------------------------------------------------------

RedisConnectionWorker::RedisConnectionWorker(const RedisConnectionConfig& config,
                                             QObject* parent)
    : QObject(parent)
    , m_config(config)
    , m_allKeys(config.allPollingKeys())
{
    // --- Gateway (subscription + command access) ---
    m_gateway = new RedisGateway(this);

    connect(m_gateway, &RedisGateway::messageReceived,
            this, &RedisConnectionWorker::onGatewayMessage);

    // --- Polling worker on a dedicated thread ---
    m_pollThread = new QThread(this);
    m_pollThread->setObjectName(
        QStringLiteral("poll-%1").arg(m_config.connectionId));

    m_pollWorker = new RedisPollingWorker(m_config.host, m_config.port);
    m_pollWorker->moveToThread(m_pollThread);

    // Connect poll results back to the main thread (queued automatically
    // because worker lives on a different thread).
    connect(m_pollWorker, &RedisPollingWorker::keyValuesReceived,
            this, &RedisConnectionWorker::onPollResult,
            Qt::QueuedConnection);

    // Cross-thread poll trigger: requestPoll() → m_pollWorker->readKeys()
    connect(this, &RedisConnectionWorker::requestPoll,
            m_pollWorker, &RedisPollingWorker::readKeys,
            Qt::QueuedConnection);

    // Select the correct DB on the poll worker when the thread starts.
    connect(m_pollThread, &QThread::started, m_pollWorker, [this]() {
        m_pollWorker->selectDb(m_config.db);
    });

    // Poll timer lives on the main thread — fires the MGET on the worker.
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(qMax(1, m_config.pollIntervalMs));
    m_pollTimer->setSingleShot(false);
    connect(m_pollTimer, &QTimer::timeout,
            this, &RedisConnectionWorker::onPollTimerTick);
}

RedisConnectionWorker::~RedisConnectionWorker()
{
    stop();
}

QString RedisConnectionWorker::connectionId() const
{
    return m_config.connectionId;
}

const RedisConnectionConfig& RedisConnectionWorker::config() const
{
    return m_config;
}

bool RedisConnectionWorker::isRunning() const
{
    return m_running;
}

void RedisConnectionWorker::start()
{
    if (m_running) {
        return;
    }
    m_running = true;

    // Connect the gateway (subscription + command connection).
    m_gateway->connectToServer(m_config.host, m_config.port);
    m_gateway->selectDb(m_config.db);

    // Subscribe to every configured channel.
    for (const RedisSubChannel& sub : m_config.subscriptionChannels) {
        if (!sub.channel.isEmpty()) {
            m_gateway->subscribe(sub.channel);
        }
    }

    // Start the polling thread and the timer.
    if (!m_allKeys.isEmpty()) {
        m_pollThread->start();
        m_pollTimer->start();
    }

    qDebug().noquote()
        << QStringLiteral("[RedisConnectionWorker] Started connection '%1' "
                          "(%2:%3 db=%4, %5 key(s), %6 channel(s))")
               .arg(m_config.connectionId)
               .arg(m_config.host)
               .arg(m_config.port)
               .arg(m_config.db)
               .arg(m_allKeys.size())
               .arg(m_config.subscriptionChannels.size());
}

void RedisConnectionWorker::stop()
{
    if (!m_running) {
        return;
    }
    m_running = false;

    m_pollTimer->stop();

    if (m_pollThread && m_pollThread->isRunning()) {
        m_pollThread->quit();
        m_pollThread->wait(3000);
    }

    m_gateway->disconnect();

    qDebug().noquote()
        << QStringLiteral("[RedisConnectionWorker] Stopped connection '%1'")
               .arg(m_config.connectionId);
}

void RedisConnectionWorker::onPollTimerTick()
{
    if (!m_allKeys.isEmpty()) {
        emit requestPoll(m_allKeys);
    }
}

void RedisConnectionWorker::onPollResult(const QVariantMap& values)
{
    emit pollBatchReady(m_config.connectionId, values);
}

void RedisConnectionWorker::onGatewayMessage(const QString& channel,
                                              const QByteArray& rawMessage)
{
    const QString module = m_config.moduleForChannel(channel);
    if (module.isEmpty()) {
        // Channel is not in the config — ignore silently.
        return;
    }

    const ParseResult parsed = payloadFromJsonBytes(rawMessage);
    QVariantMap payload;
    if (parsed.ok) {
        payload = parsed.map;
    } else if (!rawMessage.isEmpty()) {
        // Not valid JSON — store the raw string for the handler to inspect.
        payload.insert(QStringLiteral("_raw"), QString::fromUtf8(rawMessage));
    }

    emit subscriptionReceived(m_config.connectionId, module, channel, payload);
}
