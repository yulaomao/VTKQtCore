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

QVariant replyToVariant(const redisReply* reply);

QVariant decodeStructuredVariant(const QVariant& raw)
{
    if (!raw.isValid() || raw.isNull()) {
        return QVariant();
    }

    QByteArray jsonBytes;
    if (raw.userType() == QMetaType::QByteArray) {
        jsonBytes = raw.toByteArray();
    } else if (raw.userType() == QMetaType::QString) {
        jsonBytes = raw.toString().toUtf8();
    } else {
        return raw;
    }

    if (jsonBytes.isEmpty()) {
        return raw.userType() == QMetaType::QByteArray ? QVariant(QString()) : raw;
    }

    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(jsonBytes, &err);
    if (err.error == QJsonParseError::NoError && !doc.isNull()) {
        return doc.toVariant();
    }

    return raw.userType() == QMetaType::QByteArray ? QVariant(QString::fromUtf8(jsonBytes)) : raw;
}

bool tryListIndex(const QString& segment, int* index)
{
    bool ok = false;
    const int parsed = segment.toInt(&ok);
    if (!ok || parsed < 0) {
        return false;
    }

    if (index) {
        *index = parsed;
    }
    return true;
}

QVariant extractNestedValue(QVariant current, const QStringList& path)
{
    for (const QString& segment : path) {
        current = decodeStructuredVariant(current);
        if (current.userType() == QMetaType::QVariantMap) {
            const QVariantMap map = current.toMap();
            if (!map.contains(segment)) {
                return QVariant();
            }
            current = map.value(segment);
            continue;
        }

        if (current.userType() == QMetaType::QVariantList) {
            int listIndex = -1;
            if (!tryListIndex(segment, &listIndex)) {
                return QVariant();
            }

            const QVariantList list = current.toList();
            if (listIndex < 0 || listIndex >= list.size()) {
                return QVariant();
            }
            current = list.at(listIndex);
            continue;
        }

        return QVariant();
    }

    return decodeStructuredVariant(current);
}

bool assignNestedValue(QVariant& current, const QStringList& path, const QVariant& value)
{
    if (path.isEmpty()) {
        current = value;
        return true;
    }

    current = decodeStructuredVariant(current);

    int listIndex = -1;
    const bool wantsList = tryListIndex(path.first(), &listIndex);
    if (!current.isValid() || current.isNull()) {
        current = wantsList ? QVariant(QVariantList()) : QVariant(QVariantMap());
    }

    if (current.userType() == QMetaType::QVariantMap) {
        QVariantMap map = current.toMap();
        QVariant child = map.value(path.first());
        if (!assignNestedValue(child, path.mid(1), value)) {
            return false;
        }
        map.insert(path.first(), child);
        current = map;
        return true;
    }

    if (current.userType() == QMetaType::QVariantList) {
        if (!wantsList) {
            return false;
        }

        QVariantList list = current.toList();
        while (list.size() <= listIndex) {
            list.append(QVariant());
        }

        QVariant child = list.at(listIndex);
        if (!assignNestedValue(child, path.mid(1), value)) {
            return false;
        }
        list[listIndex] = child;
        current = list;
        return true;
    }

    return false;
}

QVariantMap replyToHashMap(const redisReply* reply)
{
    QVariantMap result;
    if (!reply || reply->type != REDIS_REPLY_ARRAY) {
        return result;
    }

    for (size_t index = 0; index + 1 < reply->elements; index += 2) {
        const QString fieldName = replyString(reply->element[index]);
        const QVariant fieldValue = decodeStructuredVariant(replyToVariant(reply->element[index + 1]));
        result.insert(fieldName, fieldValue);
    }

    return result;
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

QVariant RedisLogicCommandAccess::readHashValue(const QString& hashKey, const QString& field)
{
    return readHashValue(QStringList{hashKey, field});
}

QString RedisLogicCommandAccess::readHashStringValue(const QString& hashKey, const QString& field)
{
    return readHashValue(hashKey, field).toString();
}

QVariantMap RedisLogicCommandAccess::readHashJsonValue(const QString& hashKey, const QString& field)
{
    return readHashValue(QStringList{hashKey, field}).toMap();
}

QVariant RedisLogicCommandAccess::readHashValue(const QStringList& path)
{
    if (path.isEmpty()) {
        emit errorOccurred(QStringLiteral("Redis hash path is empty"));
        return QVariant();
    }

    QString errorMessage;
    QVariant result;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!ensureConnectedLocked(&errorMessage)) {
            result = QVariant();
        } else if (path.size() == 1) {
            const QByteArray hashKeyBytes = path.first().toUtf8();
            redisReply* reply = static_cast<redisReply*>(redisCommand(
                m_context,
                "HGETALL %b",
                hashKeyBytes.constData(), static_cast<size_t>(hashKeyBytes.size())));

            if (!reply) {
                errorMessage = redisErrorMessage(
                    m_context,
                    QStringLiteral("Timed out reading Redis hash '%1'").arg(path.first()));
                closeContextLocked();
            } else {
                std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(reply, &freeReplyObject);
                if (reply->type == REDIS_REPLY_ERROR) {
                    errorMessage = QStringLiteral("Redis HGETALL failed for hash '%1': %2")
                                       .arg(path.first(), replyString(reply));
                } else {
                    result = replyToHashMap(reply);
                }
            }
        } else {
            const QString hashKey = path.at(0);
            const QString field = path.at(1);
            const QByteArray hashKeyBytes = hashKey.toUtf8();
            const QByteArray fieldBytes = field.toUtf8();
            redisReply* reply = static_cast<redisReply*>(redisCommand(
                m_context,
                "HGET %b %b",
                hashKeyBytes.constData(), static_cast<size_t>(hashKeyBytes.size()),
                fieldBytes.constData(), static_cast<size_t>(fieldBytes.size())));

            if (!reply) {
                errorMessage = redisErrorMessage(
                    m_context,
                    QStringLiteral("Timed out reading Redis hash '%1' field '%2'")
                        .arg(hashKey, field));
                closeContextLocked();
            } else {
                std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(reply, &freeReplyObject);
                if (reply->type == REDIS_REPLY_ERROR) {
                    errorMessage = QStringLiteral("Redis HGET failed for hash '%1' field '%2': %3")
                                       .arg(hashKey, field, replyString(reply));
                } else {
                    result = decodeStructuredVariant(replyToVariant(reply));
                    if (path.size() > 2) {
                        result = extractNestedValue(result, path.mid(2));
                    }
                }
            }
        }
    }

    if (!errorMessage.isEmpty()) {
        emit errorOccurred(errorMessage);
    }

    return result;
}

QString RedisLogicCommandAccess::readHashStringValue(const QStringList& path)
{
    return readHashValue(path).toString();
}

QVariantMap RedisLogicCommandAccess::readHashJsonValue(const QStringList& path)
{
    return readHashValue(path).toMap();
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

bool RedisLogicCommandAccess::writeHashValue(const QStringList& path, const QVariant& value)
{
    if (path.isEmpty()) {
        emit errorOccurred(QStringLiteral("Redis hash path is empty"));
        return false;
    }

    QString errorMessage;
    bool success = false;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!ensureConnectedLocked(&errorMessage)) {
            success = false;
        } else if (path.size() == 1) {
            const QVariantMap wholeHash = value.toMap();
            const QByteArray hashKeyBytes = path.first().toUtf8();

            redisReply* deleteReply = static_cast<redisReply*>(redisCommand(
                m_context,
                "DEL %b",
                hashKeyBytes.constData(), static_cast<size_t>(hashKeyBytes.size())));
            if (!deleteReply) {
                errorMessage = redisErrorMessage(
                    m_context,
                    QStringLiteral("Failed clearing Redis hash '%1'").arg(path.first()));
                closeContextLocked();
            } else {
                freeReplyObject(deleteReply);
            }

            if (errorMessage.isEmpty()) {
                success = true;
                for (auto it = wholeHash.cbegin(); it != wholeHash.cend(); ++it) {
                    const QByteArray fieldBytes = it.key().toUtf8();
                    const QByteArray payload = variantToRedisBytes(it.value());
                    redisReply* reply = static_cast<redisReply*>(redisCommand(
                        m_context,
                        "HSET %b %b %b",
                        hashKeyBytes.constData(), static_cast<size_t>(hashKeyBytes.size()),
                        fieldBytes.constData(), static_cast<size_t>(fieldBytes.size()),
                        payload.constData(), static_cast<size_t>(payload.size())));
                    if (!reply) {
                        errorMessage = redisErrorMessage(
                            m_context,
                            QStringLiteral("Failed writing Redis hash '%1' field '%2'")
                                .arg(path.first(), it.key()));
                        closeContextLocked();
                        success = false;
                        break;
                    }

                    std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(reply, &freeReplyObject);
                    if (reply->type == REDIS_REPLY_ERROR) {
                        errorMessage = QStringLiteral("Redis HSET failed for hash '%1' field '%2': %3")
                                           .arg(path.first(), it.key(), replyString(reply));
                        success = false;
                        break;
                    }
                }
            }
        } else if (path.size() == 2) {
            const QByteArray hashKeyBytes = path.at(0).toUtf8();
            const QByteArray fieldBytes = path.at(1).toUtf8();
            const QByteArray payload = variantToRedisBytes(value);
            redisReply* reply = static_cast<redisReply*>(redisCommand(
                m_context,
                "HSET %b %b %b",
                hashKeyBytes.constData(), static_cast<size_t>(hashKeyBytes.size()),
                fieldBytes.constData(), static_cast<size_t>(fieldBytes.size()),
                payload.constData(), static_cast<size_t>(payload.size())));

            if (!reply) {
                errorMessage = redisErrorMessage(
                    m_context,
                    QStringLiteral("Failed writing Redis hash '%1' field '%2'")
                        .arg(path.at(0), path.at(1)));
                closeContextLocked();
            } else {
                std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(reply, &freeReplyObject);
                if (reply->type == REDIS_REPLY_ERROR) {
                    errorMessage = QStringLiteral("Redis HSET failed for hash '%1' field '%2': %3")
                                       .arg(path.at(0), path.at(1), replyString(reply));
                } else {
                    success = true;
                }
            }
        } else {
            const QByteArray hashKeyBytes = path.at(0).toUtf8();
            const QByteArray fieldBytes = path.at(1).toUtf8();
            redisReply* readReply = static_cast<redisReply*>(redisCommand(
                m_context,
                "HGET %b %b",
                hashKeyBytes.constData(), static_cast<size_t>(hashKeyBytes.size()),
                fieldBytes.constData(), static_cast<size_t>(fieldBytes.size())));

            if (!readReply) {
                errorMessage = redisErrorMessage(
                    m_context,
                    QStringLiteral("Failed reading Redis hash '%1' field '%2'")
                        .arg(path.at(0), path.at(1)));
                closeContextLocked();
            } else {
                std::unique_ptr<redisReply, decltype(&freeReplyObject)> readGuard(readReply, &freeReplyObject);
                if (readReply->type == REDIS_REPLY_ERROR) {
                    errorMessage = QStringLiteral("Redis HGET failed for hash '%1' field '%2': %3")
                        .arg(path.at(0), path.at(1), replyString(readReply));
                } else {
                    QVariant root = decodeStructuredVariant(replyToVariant(readReply));
                    if (!assignNestedValue(root, path.mid(2), value)) {
                        errorMessage = QStringLiteral("Failed applying nested Redis hash path '%1'")
                            .arg(path.join(QStringLiteral("/")));
                    } else {
                        const QByteArray payload = variantToRedisBytes(root);
                        redisReply* reply = static_cast<redisReply*>(redisCommand(
                            m_context,
                            "HSET %b %b %b",
                            hashKeyBytes.constData(), static_cast<size_t>(hashKeyBytes.size()),
                            fieldBytes.constData(), static_cast<size_t>(fieldBytes.size()),
                            payload.constData(), static_cast<size_t>(payload.size())));
                        if (!reply) {
                            errorMessage = redisErrorMessage(
                                m_context,
                                QStringLiteral("Failed writing Redis hash '%1' field '%2'")
                                    .arg(path.at(0), path.at(1)));
                            closeContextLocked();
                        } else {
                            std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(reply, &freeReplyObject);
                            if (reply->type == REDIS_REPLY_ERROR) {
                                errorMessage = QStringLiteral("Redis HSET failed for hash '%1' field '%2': %3")
                                    .arg(path.at(0), path.at(1), replyString(reply));
                            } else {
                                success = true;
                            }
                        }
                    }
                }
            }
        }
    }

    if (!errorMessage.isEmpty()) {
        emit errorOccurred(errorMessage);
    }

    return success;
}

bool RedisLogicCommandAccess::writeHashJsonValue(const QStringList& path, const QVariantMap& value)
{
    if (path.size() <= 1) {
        return writeHashValue(path, value);
    }

    const QJsonDocument document(QJsonObject::fromVariantMap(value));
    return writeHashValue(path, document.toJson(QJsonDocument::Compact));
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