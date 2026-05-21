#include "RedisGateway.h"

#include <hiredis/hiredis.h>

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QTimer>

#include <memory>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
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

int waitForReadable(redisFD fd, int timeoutMs)
{
    if (fd == REDIS_INVALID_FD) {
        return -1;
    }

    fd_set readSet;
    FD_ZERO(&readSet);

    timeval timeout;
    timeout.tv_sec = timeoutMs / 1000;
    timeout.tv_usec = (timeoutMs % 1000) * 1000;

#ifdef _WIN32
    FD_SET(static_cast<SOCKET>(fd), &readSet);
    return ::select(0, &readSet, nullptr, nullptr, &timeout);
#else
    FD_SET(fd, &readSet);
    return ::select(fd + 1, &readSet, nullptr, nullptr, &timeout);
#endif
}

} // namespace

RedisGateway::RedisGateway(QObject* parent)
    : QObject(parent)
    , m_reconnectTimer(new QTimer(this))
{
    m_reconnectTimer->setInterval(m_reconnectDelayMs);
    connect(m_reconnectTimer, &QTimer::timeout,
            this, &RedisGateway::onReconnectTimeout);
}

RedisGateway::~RedisGateway()
{
    disconnect();
}

void RedisGateway::connectToServer(const QString& host, int port)
{
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        m_host = host;
        m_port = port;
    }

    m_manualDisconnect.store(false);
    m_shutdownRequested.store(false);
    attemptConnection();
}

void RedisGateway::disconnect()
{
    m_manualDisconnect.store(true);
    m_shutdownRequested.store(true);

    if (m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }

    stopSubscriberWorker();
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        clearCommandContextLocked();
    }
    clearPendingSubscriptionCommands();
    setConnectionState(Disconnected);
}

RedisGateway::ConnectionState RedisGateway::getConnectionState() const
{
    return m_connectionState;
}

QString RedisGateway::getHost() const
{
    std::lock_guard<std::mutex> lock(m_commandMutex);
    return m_host;
}

int RedisGateway::getPort() const
{
    std::lock_guard<std::mutex> lock(m_commandMutex);
    return m_port;
}

bool RedisGateway::selectDb(int db)
{
    QString errorMessage;
    bool connectionLost = false;

    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        if (!m_commandContext) {
            errorMessage = QStringLiteral("Redis command connection is not connected");
            connectionLost = true;
        } else {
            redisReply* reply = static_cast<redisReply*>(
                redisCommand(m_commandContext, "SELECT %d", db));
            if (!reply) {
                errorMessage = redisErrorMessage(
                    m_commandContext,
                    QStringLiteral("Redis SELECT %1 failed").arg(db));
                clearCommandContextLocked();
                connectionLost = true;
            } else {
                if (reply->type == REDIS_REPLY_ERROR) {
                    errorMessage = QStringLiteral("Redis SELECT %1 error: %2")
                        .arg(db)
                        .arg(replyString(reply));
                }
                freeReplyObject(reply);
            }
        }
    }

    if (!errorMessage.isEmpty()) {
        if (connectionLost) {
            handleConnectionFailure(errorMessage);
        } else {
            emit errorOccurred(errorMessage);
        }
        return false;
    }

    return true;
}

bool RedisGateway::waitForConnected(int timeoutMs)
{
    if (m_connectionState == Connected) {
        return true;
    }

    if (timeoutMs <= 0) {
        return false;
    }

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);

    QMetaObject::Connection stateConnection;
    stateConnection = connect(this, &RedisGateway::connectionStateChanged,
                              &loop, [&](ConnectionState state) {
                                  if (state == Connected) {
                                      loop.quit();
                                  }
                              });
    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);

    timeoutTimer.start(timeoutMs);
    loop.exec();
    QObject::disconnect(stateConnection);
    return m_connectionState == Connected;
}

void RedisGateway::subscribe(const QString& channel)
{
    if (channel.isEmpty()) {
        return;
    }

    bool inserted = false;
    {
        std::lock_guard<std::mutex> lock(m_subscriptionMutex);
        inserted = !m_subscribedChannels.contains(channel);
        if (inserted) {
            m_subscribedChannels.insert(channel);
        }
    }

    if (inserted && m_connectionState == Connected) {
        queueSubscriptionCommand(SubscriptionCommandType::Subscribe, channel);
    }
}

void RedisGateway::unsubscribe(const QString& channel)
{
    if (channel.isEmpty()) {
        return;
    }

    bool removed = false;
    {
        std::lock_guard<std::mutex> lock(m_subscriptionMutex);
        removed = m_subscribedChannels.remove(channel);
    }

    if (removed && m_connectionState == Connected) {
        queueSubscriptionCommand(SubscriptionCommandType::Unsubscribe, channel);
    }
}

bool RedisGateway::publish(const QString& channel, const QByteArray& message)
{
    if (channel.isEmpty()) {
        return false;
    }

    const QByteArray channelBytes = channel.toUtf8();
    redisReply* reply = nullptr;
    QString errorMessage;
    bool connectionLost = false;

    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        if (!m_commandContext) {
            errorMessage = QStringLiteral("Redis command connection is not connected");
            connectionLost = true;
        } else {
            reply = static_cast<redisReply*>(redisCommand(
                m_commandContext,
                "PUBLISH %b %b",
                channelBytes.constData(), static_cast<size_t>(channelBytes.size()),
                message.constData(), static_cast<size_t>(message.size())));
            if (!reply) {
                errorMessage = redisErrorMessage(
                    m_commandContext,
                    QStringLiteral("Redis publish failed for channel '%1'").arg(channel));
                clearCommandContextLocked();
                connectionLost = true;
            }
        }
    }

    if (reply) {
        if (reply->type == REDIS_REPLY_ERROR) {
            errorMessage = QStringLiteral("Redis publish failed: %1").arg(replyString(reply));
        }
        freeReplyObject(reply);
    }

    if (!errorMessage.isEmpty()) {
        if (connectionLost) {
            handleConnectionFailure(errorMessage);
        } else {
            emit errorOccurred(errorMessage);
        }
        return false;
    }

    return true;
}

QVariant RedisGateway::readKey(const QString& key)
{
    QString errorMessage;
    QVariant result;
    bool connectionLost = false;

    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        if (!m_commandContext) {
            errorMessage = QStringLiteral("Redis command connection is not connected");
            connectionLost = true;
        } else {
            result = executeGet(m_commandContext, key, &errorMessage);
            if (!errorMessage.isEmpty() && m_commandContext->err != REDIS_OK) {
                clearCommandContextLocked();
                connectionLost = true;
            }
        }
    }

    if (!errorMessage.isEmpty()) {
        if (connectionLost) {
            handleConnectionFailure(errorMessage);
        } else {
            emit errorOccurred(errorMessage);
        }
        return QVariant();
    }

    return result;
}

bool RedisGateway::writeKey(const QString& key, const QVariant& value)
{
    if (key.isEmpty()) {
        return false;
    }

    QString errorMessage;
    bool connectionLost = false;
    bool success = false;
    const QByteArray payload = variantToRedisBytes(value);

    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        if (!m_commandContext) {
            errorMessage = QStringLiteral("Redis command connection is not connected");
            connectionLost = true;
        } else {
            success = executeSet(m_commandContext, key, payload, &errorMessage);
            if (!success && !errorMessage.isEmpty() && m_commandContext->err != REDIS_OK) {
                clearCommandContextLocked();
                connectionLost = true;
            }
        }
    }

    if (!errorMessage.isEmpty()) {
        if (connectionLost) {
            handleConnectionFailure(errorMessage);
        } else {
            emit errorOccurred(errorMessage);
        }
        return false;
    }

    return success;
}

bool RedisGateway::writeJson(const QString& key, const QVariantMap& value)
{
    const QJsonDocument document(QJsonObject::fromVariantMap(value));
    return writeKey(key, document.toJson(QJsonDocument::Compact));
}

QString RedisGateway::readString(const QString& key)
{
    return readKey(key).toString();
}

QVariantMap RedisGateway::readJson(const QString& key)
{
    const QVariant value = readKey(key);
    const QByteArray json = value.toByteArray();
    if (!json.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(json);
        if (doc.isObject()) {
            return doc.object().toVariantMap();
        }
    }

    return value.toMap();
}

void RedisGateway::onReconnectTimeout()
{
    if (m_manualDisconnect.load() || m_shutdownRequested.load()) {
        return;
    }

    attemptConnection();
}

void RedisGateway::setConnectionState(ConnectionState state)
{
    if (m_connectionState == state) {
        return;
    }

    m_connectionState = state;
    emit connectionStateChanged(m_connectionState);
}

void RedisGateway::attemptConnection()
{
    QString host;
    int port = 0;
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        host = m_host;
        port = m_port;
    }

    if (host.isEmpty() || port <= 0) {
        emit errorOccurred(QStringLiteral("Redis host or port is not configured"));
        return;
    }

    setConnectionState(Reconnecting);

    QString commandError;
    std::unique_ptr<redisContext, decltype(&redisFree)> newCommandContext(createContext(&commandError), &redisFree);
    if (!newCommandContext) {
        emit errorOccurred(commandError);
        if (!m_reconnectTimer->isActive()) {
            m_reconnectTimer->start();
        }
        return;
    }

    QString subscriberError;
    std::unique_ptr<redisContext, decltype(&redisFree)> newSubscriberContext(createContext(&subscriberError), &redisFree);
    if (!newSubscriberContext) {
        emit errorOccurred(subscriberError);
        if (!m_reconnectTimer->isActive()) {
            m_reconnectTimer->start();
        }
        return;
    }

    stopSubscriberWorker();
    clearPendingSubscriptionCommands();

    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        clearCommandContextLocked();
        m_commandContext = newCommandContext.release();
    }

    m_subscriberContext = newSubscriberContext.release();
    m_stopSubscriber.store(false);
    m_subscriberThread = std::thread(&RedisGateway::subscriberLoop, this);

    if (m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }
    setConnectionState(Connected);
}

redisContext* RedisGateway::createContext(QString* errorMessage) const
{
    QString host;
    int port = 0;
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        host = m_host;
        port = m_port;
    }

    if (host.isEmpty() || port <= 0) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Redis host or port is not configured");
        }
        return nullptr;
    }

    const QByteArray hostBytes = host.toUtf8();
    const timeval timeout {
        m_connectTimeoutMs / 1000,
        (m_connectTimeoutMs % 1000) * 1000
    };

    redisContext* context = redisConnectWithTimeout(hostBytes.constData(), port, timeout);
    if (!context) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Failed to allocate hiredis context for %1:%2")
                                .arg(host)
                                .arg(port);
        }
        return nullptr;
    }

    if (context->err != REDIS_OK) {
        if (errorMessage) {
            *errorMessage = redisErrorMessage(
                context,
                QStringLiteral("Failed to connect to Redis at %1:%2").arg(host).arg(port));
        }
        redisFree(context);
        return nullptr;
    }

    redisEnableKeepAlive(context);
    redisSetTimeout(context, timeout);
    return context;
}

void RedisGateway::clearCommandContextLocked()
{
    if (m_commandContext) {
        redisFree(m_commandContext);
        m_commandContext = nullptr;
    }
}

void RedisGateway::clearPendingSubscriptionCommands()
{
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);
    m_pendingSubscriptionCommands.clear();
}

void RedisGateway::stopSubscriberWorker()
{
    m_stopSubscriber.store(true);
    if (m_subscriberThread.joinable()) {
        m_subscriberThread.join();
    }

    if (m_subscriberContext) {
        redisFree(m_subscriberContext);
        m_subscriberContext = nullptr;
    }
}

void RedisGateway::handleConnectionFailure(const QString& reason)
{
    emit errorOccurred(reason);

    if (m_manualDisconnect.load() || m_shutdownRequested.load()) {
        return;
    }

    stopSubscriberWorker();
    {
        std::lock_guard<std::mutex> lock(m_commandMutex);
        clearCommandContextLocked();
    }

    setConnectionState(Reconnecting);
    if (!m_reconnectTimer->isActive()) {
        m_reconnectTimer->start();
    }
}

void RedisGateway::queueSubscriptionCommand(SubscriptionCommandType type, const QString& channel)
{
    std::lock_guard<std::mutex> lock(m_subscriptionMutex);
    m_pendingSubscriptionCommands.push_back(SubscriptionCommand{type, channel});
}

void RedisGateway::subscriberLoop()
{
    redisContext* context = m_subscriberContext;
    if (!context) {
        return;
    }

    if (!processInitialSubscriptions(context)) {
        return;
    }

    while (!m_stopSubscriber.load()) {
        if (!processPendingSubscriptionCommands(context)) {
            return;
        }

        const int ready = waitForReadable(context->fd, m_subscriberPollIntervalMs);
        if (m_stopSubscriber.load()) {
            return;
        }
        if (ready < 0) {
            const QString reason = redisErrorMessage(context, QStringLiteral("Redis subscriber select failed"));
            QMetaObject::invokeMethod(this,
                                      [this, reason]() { handleConnectionFailure(reason); },
                                      Qt::QueuedConnection);
            return;
        }
        if (ready == 0) {
            continue;
        }

        if (redisBufferRead(context) != REDIS_OK) {
            const QString reason = redisErrorMessage(context, QStringLiteral("Redis subscriber read failed"));
            QMetaObject::invokeMethod(this,
                                      [this, reason]() { handleConnectionFailure(reason); },
                                      Qt::QueuedConnection);
            return;
        }

        while (!m_stopSubscriber.load()) {
            void* replyObject = nullptr;
            if (redisGetReplyFromReader(context, &replyObject) != REDIS_OK) {
                const QString reason = redisErrorMessage(context, QStringLiteral("Redis subscriber reply parsing failed"));
                QMetaObject::invokeMethod(this,
                                          [this, reason]() { handleConnectionFailure(reason); },
                                          Qt::QueuedConnection);
                return;
            }

            if (!replyObject) {
                break;
            }

            dispatchSubscriberReply(replyObject);
            freeReplyObject(replyObject);
        }
    }
}

bool RedisGateway::processInitialSubscriptions(redisContext* context)
{
    QList<QString> channels;
    {
        std::lock_guard<std::mutex> lock(m_subscriptionMutex);
        channels = m_subscribedChannels.values();
    }

    for (const QString& channel : channels) {
        if (!appendSubscriptionCommand(context, SubscriptionCommand{SubscriptionCommandType::Subscribe, channel})) {
            return false;
        }
    }

    return true;
}

bool RedisGateway::processPendingSubscriptionCommands(redisContext* context)
{
    std::deque<SubscriptionCommand> commands;
    {
        std::lock_guard<std::mutex> lock(m_subscriptionMutex);
        commands.swap(m_pendingSubscriptionCommands);
    }

    for (const SubscriptionCommand& command : commands) {
        if (!appendSubscriptionCommand(context, command)) {
            return false;
        }
    }

    return true;
}

bool RedisGateway::appendSubscriptionCommand(redisContext* context, const SubscriptionCommand& command)
{
    const QByteArray channelBytes = command.channel.toUtf8();
    const QByteArray commandBytes =
        command.type == SubscriptionCommandType::Subscribe ? QByteArrayLiteral("SUBSCRIBE")
                                                           : QByteArrayLiteral("UNSUBSCRIBE");

    const char* argv[] = {commandBytes.constData(), channelBytes.constData()};
    const size_t argvLen[] = {
        static_cast<size_t>(commandBytes.size()),
        static_cast<size_t>(channelBytes.size())
    };

    if (redisAppendCommandArgv(context, 2, argv, argvLen) != REDIS_OK) {
        const QString reason = redisErrorMessage(
            context,
            QStringLiteral("Redis subscriber command queue failed for channel '%1'").arg(command.channel));
        QMetaObject::invokeMethod(this,
                                  [this, reason]() { handleConnectionFailure(reason); },
                                  Qt::QueuedConnection);
        return false;
    }

    int done = 0;
    while (!done) {
        if (redisBufferWrite(context, &done) != REDIS_OK) {
            const QString reason = redisErrorMessage(
                context,
                QStringLiteral("Redis subscriber command flush failed for channel '%1'").arg(command.channel));
            QMetaObject::invokeMethod(this,
                                      [this, reason]() { handleConnectionFailure(reason); },
                                      Qt::QueuedConnection);
            return false;
        }
    }

    return true;
}

void RedisGateway::dispatchSubscriberReply(void* replyObject)
{
    const redisReply* reply = static_cast<const redisReply*>(replyObject);
    if (!reply || reply->type != REDIS_REPLY_ARRAY || reply->elements < 1) {
        return;
    }

    const QString messageType = replyString(reply->element[0]).toLower();
    if (messageType == QStringLiteral("message") && reply->elements >= 3) {
        const QString channel = replyString(reply->element[1]);
        const QByteArray payload = replyBytes(reply->element[2]);
        QMetaObject::invokeMethod(this,
                                  [this, channel, payload]() {
                                      emit messageReceived(channel, payload);
                                  },
                                  Qt::QueuedConnection);
    }
}

bool RedisGateway::executeSet(redisContext* context, const QString& key, const QByteArray& value,
                              QString* errorMessage) const
{
    const QByteArray keyBytes = key.toUtf8();
    redisReply* reply = static_cast<redisReply*>(redisCommand(
        context,
        "SET %b %b",
        keyBytes.constData(), static_cast<size_t>(keyBytes.size()),
        value.constData(), static_cast<size_t>(value.size())));

    if (!reply) {
        if (errorMessage) {
            *errorMessage = redisErrorMessage(
                context,
                QStringLiteral("Failed writing Redis key '%1'").arg(key));
        }
        return false;
    }

    std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(reply, &freeReplyObject);
    if (reply->type == REDIS_REPLY_ERROR) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Redis command failed for key '%1': %2")
                                .arg(key, replyString(reply));
        }
        return false;
    }

    return true;
}

QVariant RedisGateway::executeGet(redisContext* context, const QString& key, QString* errorMessage) const
{
    const QByteArray keyBytes = key.toUtf8();
    redisReply* reply = static_cast<redisReply*>(redisCommand(
        context,
        "GET %b",
        keyBytes.constData(), static_cast<size_t>(keyBytes.size())));

    if (!reply) {
        if (errorMessage) {
            *errorMessage = redisErrorMessage(
                context,
                QStringLiteral("Timed out reading Redis key '%1'").arg(key));
        }
        return QVariant();
    }

    std::unique_ptr<redisReply, decltype(&freeReplyObject)> guard(reply, &freeReplyObject);
    if (reply->type == REDIS_REPLY_ERROR) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("Redis command failed for key '%1': %2")
                                .arg(key, replyString(reply));
        }
        return QVariant();
    }

    return replyToVariant(reply);
}

