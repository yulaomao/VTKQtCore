#include "RedisPollingWorker.h"

#include <hiredis/hiredis.h>

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace {

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

void RedisPollingWorker::readKey(const QString& key)
{
    if (!ensureConnected()) {
        return;
    }

    const QByteArray keyBytes = key.toUtf8();
    redisReply* reply = static_cast<redisReply*>(
        redisCommand(m_context, "GET %b",
                     keyBytes.constData(),
                     static_cast<size_t>(keyBytes.size())));

    if (!reply) {
        // Connection lost; drop context so next call reconnects.
        closeContext();
        return;
    }

    QVariant value;
    if (reply->type != REDIS_REPLY_ERROR) {
        value = replyToVariantPoll(reply);
    }
    freeReplyObject(reply);

    emit keyValueReceived(key, value);
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
    return true;
}

void RedisPollingWorker::closeContext()
{
    if (m_context) {
        redisFree(m_context);
        m_context = nullptr;
    }
}
