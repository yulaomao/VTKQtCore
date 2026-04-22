#include "RedisLogicCommandAccess.h"

#include <hiredis/hiredis.h>

#include <QJsonDocument>
#include <QJsonObject>

#include <memory>

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace {

QString redisErrorMessage(const redisContext* context, const QString& fallback)
{
    if (context && context->errstr[0] != '\0') {
        return QStringLiteral("%1: %2").arg(fallback, QString::fromUtf8(context->errstr));
    }

    return fallback;
}

QByteArray replyBytes(const redisReply* reply)
{
    if (!reply || !reply->str || reply->len == 0) {
        return QByteArray();
    }

    return QByteArray(reply->str, static_cast<int>(reply->len));
}

QString replyString(const redisReply* reply)
{
    return QString::fromUtf8(replyBytes(reply));
}

QVariant replyToVariant(const redisReply* reply)
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
        return replyBytes(reply);
    case REDIS_REPLY_STATUS:
        return replyString(reply);
    case REDIS_REPLY_INTEGER:
        return static_cast<qlonglong>(reply->integer);
    case REDIS_REPLY_DOUBLE:
        return reply->str ? QVariant(QString::fromUtf8(reply->str)) : QVariant(reply->dval);
    case REDIS_REPLY_BOOL:
        return reply->integer != 0;
    case REDIS_REPLY_ARRAY:
    case REDIS_REPLY_SET:
    case REDIS_REPLY_PUSH:
    case REDIS_REPLY_MAP: {
        QVariantList list;
        for (size_t index = 0; index < reply->elements; ++index) {
            list.append(replyToVariant(reply->element[index]));
        }
        return list;
    }
    default:
        return replyString(reply);
    }
}

QByteArray variantToRedisBytes(const QVariant& value)
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

    if (value.canConvert<QVariantMap>() || value.canConvert<QVariantList>() ||
        value.canConvert<QStringList>()) {
        const QJsonDocument document = QJsonDocument::fromVariant(value);
        if (!document.isNull()) {
            return document.toJson(QJsonDocument::Compact);
        }
    }

    return value.toString().toUtf8();
}

}

RedisLogicCommandAccess::RedisLogicCommandAccess(QObject* parent)
    : QObject(parent)
{
}

RedisLogicCommandAccess::~RedisLogicCommandAccess()
{
    disconnect();
}

void RedisLogicCommandAccess::connectToServer(const QString& host, int port, int db)
{
    QString errorMessage;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_host = host;
        m_port = port;
        m_db = db;
        closeContextLocked();
        ensureConnectedLocked(&errorMessage);
    }

    if (!errorMessage.isEmpty()) {
        emit errorOccurred(errorMessage);
    }
}

void RedisLogicCommandAccess::disconnect()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    closeContextLocked();
}

bool RedisLogicCommandAccess::isAvailable() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_context && m_context->err == REDIS_OK;
}

QVariant RedisLogicCommandAccess::readValue(const QString& key)
{
    QString errorMessage;
    QVariant result;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!ensureConnectedLocked(&errorMessage)) {
            result = QVariant();
        } else {
            const QByteArray keyBytes = key.toUtf8();
            redisReply* reply = static_cast<redisReply*>(redisCommand(
                m_context,
                "GET %b",
                keyBytes.constData(), static_cast<size_t>(keyBytes.size())));

            if (!reply) {
                errorMessage = redisErrorMessage(
                    m_context,
                    QStringLiteral("Timed out reading Redis key '%1'").arg(key));
                closeContextLocked();
            } else {
                std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(reply, &freeReplyObject);
                if (reply->type == REDIS_REPLY_ERROR) {
                    errorMessage = QStringLiteral("Redis command failed for key '%1': %2")
                                       .arg(key, replyString(reply));
                } else {
                    result = replyToVariant(reply);
                }
            }
        }
    }

    if (!errorMessage.isEmpty()) {
        emit errorOccurred(errorMessage);
    }

    return result;
}

QString RedisLogicCommandAccess::readStringValue(const QString& key)
{
    return readValue(key).toString();
}

QVariantMap RedisLogicCommandAccess::readJsonValue(const QString& key)
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

bool RedisLogicCommandAccess::writeValue(const QString& key, const QVariant& value)
{
    QString errorMessage;
    bool success = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!ensureConnectedLocked(&errorMessage)) {
            success = false;
        } else {
            const QByteArray keyBytes = key.toUtf8();
            const QByteArray payload = variantToRedisBytes(value);
            redisReply* reply = static_cast<redisReply*>(redisCommand(
                m_context,
                "SET %b %b",
                keyBytes.constData(), static_cast<size_t>(keyBytes.size()),
                payload.constData(), static_cast<size_t>(payload.size())));

            if (!reply) {
                errorMessage = redisErrorMessage(
                    m_context,
                    QStringLiteral("Failed writing Redis key '%1'").arg(key));
                closeContextLocked();
            } else {
                std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(reply, &freeReplyObject);
                if (reply->type == REDIS_REPLY_ERROR) {
                    errorMessage = QStringLiteral("Redis command failed for key '%1': %2")
                                       .arg(key, replyString(reply));
                } else {
                    success = true;
                }
            }
        }
    }

    if (!errorMessage.isEmpty()) {
        emit errorOccurred(errorMessage);
    }

    return success;
}

bool RedisLogicCommandAccess::writeJsonValue(const QString& key, const QVariantMap& value)
{
    const QJsonDocument document(QJsonObject::fromVariantMap(value));
    return writeValue(key, document.toJson(QJsonDocument::Compact));
}

bool RedisLogicCommandAccess::publishMessage(const QString& channel, const QByteArray& message)
{
    QString errorMessage;
    bool success = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!ensureConnectedLocked(&errorMessage)) {
            success = false;
        } else {
            const QByteArray channelBytes = channel.toUtf8();
            redisReply* reply = static_cast<redisReply*>(redisCommand(
                m_context,
                "PUBLISH %b %b",
                channelBytes.constData(), static_cast<size_t>(channelBytes.size()),
                message.constData(), static_cast<size_t>(message.size())));

            if (!reply) {
                errorMessage = redisErrorMessage(
                    m_context,
                    QStringLiteral("Redis publish failed for channel '%1'").arg(channel));
                closeContextLocked();
            } else {
                std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(reply, &freeReplyObject);
                if (reply->type == REDIS_REPLY_ERROR) {
                    errorMessage = QStringLiteral("Redis publish failed: %1").arg(replyString(reply));
                } else {
                    success = true;
                }
            }
        }
    }

    if (!errorMessage.isEmpty()) {
        emit errorOccurred(errorMessage);
    }

    return success;
}

bool RedisLogicCommandAccess::publishJsonMessage(const QString& channel, const QVariantMap& payload)
{
    const QJsonDocument document(QJsonObject::fromVariantMap(payload));
    return publishMessage(channel, document.toJson(QJsonDocument::Compact));
}

redisContext* RedisLogicCommandAccess::createContext(QString* errorMessage) const
{
    if (m_host.isEmpty() || m_port <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Redis host or port is not configured");
        }
        return nullptr;
    }

    const QByteArray hostBytes = m_host.toUtf8();
    const timeval timeout {
        m_connectTimeoutMs / 1000,
        (m_connectTimeoutMs % 1000) * 1000
    };

    redisContext* context = redisConnectWithTimeout(hostBytes.constData(), m_port, timeout);
    if (!context) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to allocate hiredis context for %1:%2")
                                .arg(m_host)
                                .arg(m_port);
        }
        return nullptr;
    }

    if (context->err != REDIS_OK) {
        if (errorMessage) {
            *errorMessage = redisErrorMessage(
                context,
                QStringLiteral("Failed to connect to Redis at %1:%2").arg(m_host).arg(m_port));
        }
        redisFree(context);
        return nullptr;
    }

    if (m_db != 0) {
        redisReply* reply = static_cast<redisReply*>(redisCommand(context, "SELECT %d", m_db));
        if (!reply) {
            if (errorMessage) {
                *errorMessage = redisErrorMessage(
                    context,
                    QStringLiteral("Failed to select Redis db %1 at %2:%3")
                        .arg(m_db)
                        .arg(m_host)
                        .arg(m_port));
            }
            redisFree(context);
            return nullptr;
        }

        std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(reply, &freeReplyObject);
        if (reply->type == REDIS_REPLY_ERROR) {
            if (errorMessage) {
                *errorMessage = QStringLiteral("Redis SELECT %1 error: %2")
                                    .arg(m_db)
                                    .arg(replyString(reply));
            }
            redisFree(context);
            return nullptr;
        }
    }

    redisEnableKeepAlive(context);
    redisSetTimeout(context, timeout);
    return context;
}

void RedisLogicCommandAccess::closeContextLocked()
{
    if (m_context) {
        redisFree(m_context);
        m_context = nullptr;
    }
}

bool RedisLogicCommandAccess::ensureConnectedLocked(QString* errorMessage)
{
    if (m_context && m_context->err == REDIS_OK) {
        return true;
    }

    closeContextLocked();
    m_context = createContext(errorMessage);
    return m_context != nullptr;
}