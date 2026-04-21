#include "RedisPollingWorker.h"

#include <hiredis/hiredis.h>

#include <algorithm>
#include <QVector>

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

void RedisPollingWorker::readKeys(const QStringList& keys)
{
    if (keys.isEmpty()) {
        emit keyValuesReceived({});
        return;
    }

    if (!ensureConnected()) {
        return;
    }

    QVector<QByteArray> commandParts;
    commandParts.reserve(keys.size() + 1);
    commandParts.append(QByteArrayLiteral("MGET"));
    for (const QString& key : keys) {
        commandParts.append(key.toUtf8());
    }

    QVector<const char*> argv;
    QVector<size_t> argvlen;
    argv.reserve(commandParts.size());
    argvlen.reserve(commandParts.size());
    for (const QByteArray& part : commandParts) {
        argv.append(part.constData());
        argvlen.append(static_cast<size_t>(part.size()));
    }

    redisReply* reply = static_cast<redisReply*>(
        redisCommandArgv(m_context,
                         commandParts.size(),
                         argv.data(),
                         argvlen.data()));

    if (!reply) {
        closeContext();
        return;
    }

    QVariantMap values;
    if (reply->type == REDIS_REPLY_ARRAY) {
        const size_t count = (std::min)(reply->elements, static_cast<size_t>(keys.size()));
        for (size_t index = 0; index < count; ++index) {
            values.insert(keys.at(static_cast<int>(index)), replyToVariantPoll(reply->element[index]));
        }
    }
    freeReplyObject(reply);

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
    return true;
}

void RedisPollingWorker::closeContext()
{
    if (m_context) {
        redisFree(m_context);
        m_context = nullptr;
    }
}
