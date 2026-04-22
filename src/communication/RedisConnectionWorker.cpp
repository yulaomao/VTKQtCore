#include "RedisConnectionWorker.h"

#include <hiredis/hiredis.h>

#include <QJsonObject>
#include <QTimer>
#include <QVector>

#include <algorithm>
#include <memory>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/select.h>
#endif

namespace {

QVariant replyToVariantImpl(const redisReply* reply)
{
    if (!reply) {
        return QVariant();
    }

    switch (reply->type) {
    case REDIS_REPLY_NIL:
        return QVariant();
    case REDIS_REPLY_STRING:
    case REDIS_REPLY_BIGNUM:
    case REDIS_REPLY_VERB:
        return (reply->str && reply->len > 0)
                   ? QVariant(QByteArray(reply->str, static_cast<int>(reply->len)))
                   : QVariant(QByteArray());
    case REDIS_REPLY_STATUS:
        return reply->str ? QVariant(QString::fromUtf8(reply->str)) : QVariant();
    case REDIS_REPLY_INTEGER:
        return static_cast<qlonglong>(reply->integer);
    case REDIS_REPLY_DOUBLE:
        return reply->str ? QVariant(QString::fromUtf8(reply->str)) : QVariant(reply->dval);
    case REDIS_REPLY_BOOL:
        return reply->integer != 0;
    default:
        return reply->str ? QVariant(QString::fromUtf8(reply->str)) : QVariant();
    }
}

QByteArray variantToBytesImpl(const QVariant& value)
{
    if (!value.isValid() || value.isNull()) {
        return QByteArray();
    }
    if (value.userType() == QMetaType::QByteArray) {
        return value.toByteArray();
    }
    if (value.userType() == QMetaType::QString) {
        return value.toString().toUtf8();
    }
    if (value.userType() == QMetaType::Bool) {
        return value.toBool() ? QByteArrayLiteral("true") : QByteArrayLiteral("false");
    }
    if (value.canConvert<QVariantMap>() || value.canConvert<QVariantList>()) {
        const QJsonDocument doc = QJsonDocument::fromVariant(value);
        if (!doc.isNull()) {
            return doc.toJson(QJsonDocument::Compact);
        }
    }
    return value.toString().toUtf8();
}

} // namespace

RedisConnectionWorker::RedisConnectionWorker(const RedisConnectionConfig& config,
                                             QObject* parent)
    : QObject(parent)
    , m_config(config)
{
}

RedisConnectionWorker::~RedisConnectionWorker()
{
    stop();
}

// ─── Public slots ────────────────────────────────────────────────────────────

void RedisConnectionWorker::start()
{
    if (m_started) {
        return;
    }
    m_started = true;

    // Establish the command / polling connection.
    {
        std::lock_guard<std::mutex> lock(m_cmdMutex);
        ensureCommandConnectedLocked();
    }

    // Start the Pub/Sub subscriber thread if there are channels to subscribe.
    if (!m_config.subscriptionChannels.isEmpty()) {
        m_stopSubscriber.store(false);
        m_subThread = std::thread(&RedisConnectionWorker::subscriberLoop, this);
    }

    // Start the MGET polling timer if there are key groups to poll.
    if (!m_config.pollingKeyGroups.isEmpty()) {
        m_pollTimer = new QTimer(this);
        m_pollTimer->setInterval(m_config.pollIntervalMs);
        connect(m_pollTimer, &QTimer::timeout, this, &RedisConnectionWorker::onPollTimer);
        m_pollTimer->start();
    }
}

void RedisConnectionWorker::stop()
{
    if (!m_started) {
        return;
    }
    m_started = false;

    if (m_pollTimer) {
        m_pollTimer->stop();
        delete m_pollTimer;
        m_pollTimer = nullptr;
    }

    stopSubscriber();

    {
        std::lock_guard<std::mutex> lock(m_cmdMutex);
        closeCommandContextLocked();
    }
}

// ─── IRedisCommandAccess ─────────────────────────────────────────────────────

bool RedisConnectionWorker::isAvailable() const
{
    std::lock_guard<std::mutex> lock(m_cmdMutex);
    return m_cmdContext && m_cmdContext->err == REDIS_OK;
}

QVariant RedisConnectionWorker::readValue(const QString& key)
{
    std::lock_guard<std::mutex> lock(m_cmdMutex);
    if (!ensureCommandConnectedLocked()) {
        return QVariant();
    }

    const QByteArray keyBytes = key.toUtf8();
    redisReply* reply = static_cast<redisReply*>(
        redisCommand(m_cmdContext, "GET %b", keyBytes.constData(),
                     static_cast<size_t>(keyBytes.size())));

    if (!reply) {
        closeCommandContextLocked();
        return QVariant();
    }

    std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(reply, &freeReplyObject);
    if (reply->type == REDIS_REPLY_ERROR) {
        return QVariant();
    }
    return replyToVariantImpl(reply);
}

QString RedisConnectionWorker::readStringValue(const QString& key)
{
    return readValue(key).toString();
}

QVariantMap RedisConnectionWorker::readJsonValue(const QString& key)
{
    const QVariant value = readValue(key);
    const QByteArray json = value.toByteArray();
    if (!json.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(json);
        if (doc.isObject()) {
            return doc.object().toVariantMap();
        }
    }
    return value.toMap();
}

bool RedisConnectionWorker::writeValue(const QString& key, const QVariant& value)
{
    std::lock_guard<std::mutex> lock(m_cmdMutex);
    if (!ensureCommandConnectedLocked()) {
        return false;
    }

    const QByteArray keyBytes     = key.toUtf8();
    const QByteArray payloadBytes = variantToBytesImpl(value);

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(m_cmdContext, "SET %b %b",
                     keyBytes.constData(), static_cast<size_t>(keyBytes.size()),
                     payloadBytes.constData(), static_cast<size_t>(payloadBytes.size())));

    if (!reply) {
        closeCommandContextLocked();
        return false;
    }

    std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(reply, &freeReplyObject);
    return reply->type != REDIS_REPLY_ERROR;
}

bool RedisConnectionWorker::writeJsonValue(const QString& key, const QVariantMap& value)
{
    const QJsonDocument doc(QJsonObject::fromVariantMap(value));
    return writeValue(key, doc.toJson(QJsonDocument::Compact));
}

bool RedisConnectionWorker::publishMessage(const QString& channel, const QByteArray& message)
{
    std::lock_guard<std::mutex> lock(m_cmdMutex);
    if (!ensureCommandConnectedLocked()) {
        return false;
    }

    const QByteArray channelBytes = channel.toUtf8();
    redisReply* reply = static_cast<redisReply*>(
        redisCommand(m_cmdContext, "PUBLISH %b %b",
                     channelBytes.constData(), static_cast<size_t>(channelBytes.size()),
                     message.constData(), static_cast<size_t>(message.size())));

    if (!reply) {
        closeCommandContextLocked();
        return false;
    }

    std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(reply, &freeReplyObject);
    return reply->type != REDIS_REPLY_ERROR;
}

bool RedisConnectionWorker::publishJsonMessage(const QString& channel, const QVariantMap& payload)
{
    const QJsonDocument doc(QJsonObject::fromVariantMap(payload));
    return publishMessage(channel, doc.toJson(QJsonDocument::Compact));
}

// ─── Private slot: MGET polling ──────────────────────────────────────────────

void RedisConnectionWorker::onPollTimer()
{
    struct KeyResult {
        QString module;
        QString key;
        QVariant value;
    };
    QVector<KeyResult> results;

    {
        std::lock_guard<std::mutex> lock(m_cmdMutex);
        if (!ensureCommandConnectedLocked()) {
            return;
        }

        for (const RedisKeyGroup& group : m_config.pollingKeyGroups) {
            if (group.keys.isEmpty()) {
                continue;
            }

            // Build MGET argv
            QVector<QByteArray> parts;
            parts.reserve(group.keys.size() + 1);
            parts.append(QByteArrayLiteral("MGET"));
            for (const QString& k : group.keys) {
                parts.append(k.toUtf8());
            }

            QVector<const char*> argv;
            QVector<size_t>      argvlen;
            argv.reserve(parts.size());
            argvlen.reserve(parts.size());
            for (const QByteArray& p : parts) {
                argv.append(p.constData());
                argvlen.append(static_cast<size_t>(p.size()));
            }

            redisReply* reply = static_cast<redisReply*>(
                redisCommandArgv(m_cmdContext, parts.size(),
                                 argv.data(), argvlen.data()));

            if (!reply) {
                closeCommandContextLocked();
                break;
            }

            if (reply->type == REDIS_REPLY_ARRAY) {
                const size_t count =
                    std::min(reply->elements, static_cast<size_t>(group.keys.size()));
                for (size_t i = 0; i < count; ++i) {
                    results.append({group.module,
                                    group.keys[static_cast<int>(i)],
                                    replyToVariantImpl(reply->element[i])});
                }
            }

            freeReplyObject(reply);
        }
    }

    // Emit results outside the lock.
    for (const KeyResult& r : results) {
        emit pollKeyResult(m_config.connectionId, r.module, r.key, r.value);
    }
}

// ─── Subscriber thread ────────────────────────────────────────────────────────

void RedisConnectionWorker::subscriberLoop()
{
    // Build channel → module lookup on a local copy (no locks needed after init).
    QMap<QByteArray, QString> moduleByChannel;
    for (const RedisSubscriptionChannel& sub : m_config.subscriptionChannels) {
        moduleByChannel.insert(sub.channel.toUtf8(), sub.module);
    }

    // Create a dedicated connection for the subscriber.
    redisContext* ctx = createHiredisContext(kConnectTimeoutMs, nullptr);
    if (!ctx) {
        return;
    }

    // SELECT db.
    selectDbLocked(ctx);

    // SUBSCRIBE to all configured channels.
    for (const RedisSubscriptionChannel& sub : m_config.subscriptionChannels) {
        const QByteArray channelBytes = sub.channel.toUtf8();
        redisReply* r = static_cast<redisReply*>(
            redisCommand(ctx, "SUBSCRIBE %b",
                         channelBytes.constData(),
                         static_cast<size_t>(channelBytes.size())));
        if (r) {
            freeReplyObject(r);
        }
    }

    // Read loop: use select() so we can check the stop flag without blocking forever.
    while (!m_stopSubscriber.load()) {
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(ctx->fd, &readfds);
        timeval tv{0, 200000}; // 200 ms timeout

        const int ready = select(ctx->fd + 1, &readfds, nullptr, nullptr, &tv);
        if (ready < 0) {
            break; // Socket error
        }
        if (ready == 0) {
            continue; // Timeout – check m_stopSubscriber
        }

        redisReply* reply = nullptr;
        if (redisGetReply(ctx, reinterpret_cast<void**>(&reply)) != REDIS_OK) {
            if (reply) {
                freeReplyObject(reply);
            }
            break;
        }

        if (!reply) {
            continue;
        }

        // A Pub/Sub message arrives as a 3-element array: ["message", channel, data]
        if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3 &&
            reply->element[0]->str && reply->element[1]->str && reply->element[2]->str) {
            const QLatin1String type(reply->element[0]->str, reply->element[0]->len);
            if (type == QLatin1String("message")) {
                const QByteArray channel(reply->element[1]->str,
                                         static_cast<int>(reply->element[1]->len));
                const QByteArray data(reply->element[2]->str,
                                      static_cast<int>(reply->element[2]->len));
                const QString module = moduleByChannel.value(channel);
                if (!module.isEmpty()) {
                    emit subscriptionMessage(m_config.connectionId, module,
                                             QString::fromUtf8(channel), data);
                }
            }
        }

        freeReplyObject(reply);
    }

    redisFree(ctx);
}

void RedisConnectionWorker::stopSubscriber()
{
    m_stopSubscriber.store(true);
    if (m_subThread.joinable()) {
        m_subThread.join();
    }
}

// ─── Private helpers ─────────────────────────────────────────────────────────

bool RedisConnectionWorker::ensureCommandConnectedLocked()
{
    if (m_cmdContext && m_cmdContext->err == REDIS_OK) {
        return true;
    }

    closeCommandContextLocked();

    QString errMsg;
    m_cmdContext = createHiredisContext(kConnectTimeoutMs, &errMsg);
    if (!m_cmdContext) {
        return false;
    }

    selectDbLocked(m_cmdContext);
    return true;
}

void RedisConnectionWorker::closeCommandContextLocked()
{
    if (m_cmdContext) {
        redisFree(m_cmdContext);
        m_cmdContext = nullptr;
    }
}

redisContext* RedisConnectionWorker::createHiredisContext(int connectTimeoutMs,
                                                          QString* errMsg) const
{
    if (m_config.host.isEmpty() || m_config.port <= 0) {
        if (errMsg) {
            *errMsg = QStringLiteral("Redis host or port not configured");
        }
        return nullptr;
    }

    const QByteArray hostBytes = m_config.host.toUtf8();
    const timeval timeout{connectTimeoutMs / 1000, (connectTimeoutMs % 1000) * 1000};

    redisContext* ctx = redisConnectWithTimeout(hostBytes.constData(), m_config.port, timeout);
    if (!ctx) {
        if (errMsg) {
            *errMsg = QStringLiteral("Failed to allocate hiredis context for %1:%2")
                          .arg(m_config.host)
                          .arg(m_config.port);
        }
        return nullptr;
    }

    if (ctx->err != REDIS_OK) {
        if (errMsg) {
            *errMsg = QStringLiteral("Failed to connect to Redis at %1:%2: %3")
                          .arg(m_config.host)
                          .arg(m_config.port)
                          .arg(QString::fromUtf8(ctx->errstr));
        }
        redisFree(ctx);
        return nullptr;
    }

    redisEnableKeepAlive(ctx);
    redisSetTimeout(ctx, timeout);
    return ctx;
}

bool RedisConnectionWorker::selectDbLocked(redisContext* ctx)
{
    if (!ctx || m_config.db == 0) {
        return true; // DB 0 is the default – no SELECT needed.
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommand(ctx, "SELECT %d", m_config.db));

    if (!reply) {
        return false;
    }

    const bool ok = (reply->type != REDIS_REPLY_ERROR);
    freeReplyObject(reply);
    return ok;
}

QVariant RedisConnectionWorker::replyToVariant(void* replyObject)
{
    return replyToVariantImpl(static_cast<redisReply*>(replyObject));
}

QByteArray RedisConnectionWorker::variantToBytes(const QVariant& value)
{
    return variantToBytesImpl(value);
}
