#include "RedisPollingWorker.h"

#include <hiredis/hiredis.h>

#include <algorithm>
#include <QVector>

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace {

struct PollTarget {
    QString hashKey;
    QString field;
    bool valid = false;
};

PollTarget resolvePollTarget(const QString& logicalKey)
{
    const int dotIndex = logicalKey.lastIndexOf(QLatin1Char('.'));
    const int colonIndex = logicalKey.lastIndexOf(QLatin1Char(':'));
    const int splitIndex = (std::max)(dotIndex, colonIndex);
    if (splitIndex <= 0 || splitIndex >= logicalKey.size() - 1) {
        return {};
    }

    PollTarget target;
    target.hashKey = logicalKey.left(splitIndex);
    target.field = logicalKey.mid(splitIndex + 1);
    target.valid = !target.hashKey.isEmpty() && !target.field.isEmpty();
    return target;
}

QVariant replyToVariantPoll(const redisReply* reply)
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
        return (reply->str) ? QVariant(QString::fromUtf8(reply->str)) : QVariant();
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

} // namespace

RedisPollingWorker::RedisPollingWorker(const QString& host, int port,
                                       int connectTimeoutMs,
                                       QObject* parent)
    : QObject(parent)
    , m_host(host)
    , m_port(port)
    , m_connectTimeoutMs(connectTimeoutMs)
{
}

RedisPollingWorker::~RedisPollingWorker()
{
    closeContext();
}

void RedisPollingWorker::setConnection(const QString& host, int port)
{
    if (m_host == host && m_port == port) {
        return;
    }
    m_host = host;
    m_port = port;
    // Drop existing context so ensureConnected() recreates it with new params.
    closeContext();
}

void RedisPollingWorker::setPollingKeys(const QStringList& keys)
{
    m_pollingKeys = keys;
}

void RedisPollingWorker::selectDb(int db)
{
    if (m_db == db) {
        return;
    }
    m_db = db;
    // If already connected, issue SELECT immediately; otherwise it will be
    // applied inside ensureConnected() on the next readKeys() call.
    if (m_context && m_context->err == REDIS_OK) {
        redisReply* reply = static_cast<redisReply*>(
            redisCommand(m_context, "SELECT %d", m_db));
        if (reply) {
            freeReplyObject(reply);
        } else {
            // Connection lost; drop context so it is recreated next time.
            closeContext();
        }
    }
}

void RedisPollingWorker::poll()
{
    if (m_pollingKeys.isEmpty()) {
        emit keyValuesReceived({});
        return;
    }

    if (!ensureConnected()) {
        return;
    }

    QVariantMap values;
    for (const QString& logicalKey : m_pollingKeys) {
        const PollTarget target = resolvePollTarget(logicalKey);
        if (!target.valid) {
            continue;
        }

        const QByteArray hashKeyBytes = target.hashKey.toUtf8();
        const QByteArray fieldBytes = target.field.toUtf8();
        redisReply* reply = static_cast<redisReply*>(redisCommand(
            m_context,
            "HGET %b %b",
            hashKeyBytes.constData(), static_cast<size_t>(hashKeyBytes.size()),
            fieldBytes.constData(), static_cast<size_t>(fieldBytes.size())));

        if (!reply) {
            closeContext();
            return;
        }

        values.insert(logicalKey, replyToVariantPoll(reply));
        freeReplyObject(reply);
    }

    emit keyValuesReceived(values);
}

bool RedisPollingWorker::ensureConnected()
{
    if (m_context && m_context->err == REDIS_OK) {
        return true;
    }

    closeContext();

    if (m_host.isEmpty() || m_port <= 0) {
        return false;
    }

    const QByteArray hostBytes = m_host.toUtf8();
    const timeval timeout {
        m_connectTimeoutMs / 1000,
        (m_connectTimeoutMs % 1000) * 1000
    };

    m_context = redisConnectWithTimeout(hostBytes.constData(), m_port, timeout);
    if (!m_context) {
        return false;
    }

    if (m_context->err != REDIS_OK) {
        closeContext();
        return false;
    }

    redisEnableKeepAlive(m_context);
    redisSetTimeout(m_context, timeout);

    // Select the target database (default DB 0 requires no SELECT).
    if (m_db != 0) {
        redisReply* reply = static_cast<redisReply*>(
            redisCommand(m_context, "SELECT %d", m_db));
        if (reply) {
            freeReplyObject(reply);
        } else {
            closeContext();
            return false;
        }
    }

    return true;
}

void RedisPollingWorker::closeContext()
{
    if (m_context) {
        redisFree(m_context);
        m_context = nullptr;
    }
}
