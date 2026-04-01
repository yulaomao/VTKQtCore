#include "RedisGateway.h"

#include <QAbstractSocket>
#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QJsonDocument>
#include <QTcpSocket>
#include <QTimer>

namespace {

struct RespValue {
    enum class Type {
        SimpleString,
        Error,
        Integer,
        BulkString,
        Array,
        Null
    };

    Type type = Type::Null;
    QByteArray stringValue;
    qint64 integerValue = 0;
    QList<RespValue> arrayValue;
};

QByteArray encodeRespCommand(const QList<QByteArray>& arguments)
{
    QByteArray payload;
    payload += '*';
    payload += QByteArray::number(arguments.size());
    payload += "\r\n";
    for (const QByteArray& argument : arguments) {
        payload += '$';
        payload += QByteArray::number(argument.size());
        payload += "\r\n";
        payload += argument;
        payload += "\r\n";
    }
    return payload;
}

bool parseLine(const QByteArray& buffer, int start, QByteArray& line, int& nextPos)
{
    const int separator = buffer.indexOf("\r\n", start);
    if (separator < 0) {
        return false;
    }

    line = buffer.mid(start, separator - start);
    nextPos = separator + 2;
    return true;
}

bool parseRespValue(const QByteArray& buffer, int start, RespValue& value, int& consumed)
{
    if (start >= buffer.size()) {
        return false;
    }

    const char prefix = buffer.at(start);
    QByteArray line;
    int cursor = 0;

    switch (prefix) {
    case '+':
        if (!parseLine(buffer, start + 1, line, cursor)) {
            return false;
        }
        value.type = RespValue::Type::SimpleString;
        value.stringValue = line;
        consumed = cursor;
        return true;
    case '-':
        if (!parseLine(buffer, start + 1, line, cursor)) {
            return false;
        }
        value.type = RespValue::Type::Error;
        value.stringValue = line;
        consumed = cursor;
        return true;
    case ':':
        if (!parseLine(buffer, start + 1, line, cursor)) {
            return false;
        }
        value.type = RespValue::Type::Integer;
        value.integerValue = line.toLongLong();
        consumed = cursor;
        return true;
    case '$': {
        if (!parseLine(buffer, start + 1, line, cursor)) {
            return false;
        }

        const int length = line.toInt();
        if (length < 0) {
            value.type = RespValue::Type::Null;
            consumed = cursor;
            return true;
        }

        if (cursor + length + 2 > buffer.size()) {
            return false;
        }

        value.type = RespValue::Type::BulkString;
        value.stringValue = buffer.mid(cursor, length);
        consumed = cursor + length + 2;
        return true;
    }
    case '*': {
        if (!parseLine(buffer, start + 1, line, cursor)) {
            return false;
        }

        const int count = line.toInt();
        if (count < 0) {
            value.type = RespValue::Type::Null;
            consumed = cursor;
            return true;
        }

        value.type = RespValue::Type::Array;
        value.arrayValue.clear();
        int current = cursor;
        for (int index = 0; index < count; ++index) {
            RespValue child;
            int nextConsumed = 0;
            if (!parseRespValue(buffer, current, child, nextConsumed)) {
                return false;
            }
            value.arrayValue.append(child);
            current = nextConsumed;
        }
        consumed = current;
        return true;
    }
    default:
        return false;
    }
}

QVariant respToVariant(const RespValue& value)
{
    switch (value.type) {
    case RespValue::Type::SimpleString:
        return QString::fromUtf8(value.stringValue);
    case RespValue::Type::BulkString:
        return value.stringValue;
    case RespValue::Type::Integer:
        return value.integerValue;
    case RespValue::Type::Null:
        return QVariant();
    case RespValue::Type::Array: {
        QVariantList list;
        for (const RespValue& item : value.arrayValue) {
            list.append(respToVariant(item));
        }
        return list;
    }
    case RespValue::Type::Error:
    default:
        return QString::fromUtf8(value.stringValue);
    }
}

bool isSocketConnected(const QTcpSocket* socket)
{
    return socket && socket->state() == QAbstractSocket::ConnectedState;
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

void RedisGateway::connectToServer(const QString& host, int port)
{
    m_host = host;
    m_port = port;
    m_manualDisconnect = false;
    ensureSockets();
    connectSockets();
}

void RedisGateway::disconnect()
{
    m_manualDisconnect = true;
    if (m_reconnectTimer->isActive()) {
        m_reconnectTimer->stop();
    }

    if (m_commandSocket) {
        m_commandSocket->disconnectFromHost();
        m_commandSocket->abort();
    }

    if (m_subscriberSocket) {
        m_subscriberSocket->disconnectFromHost();
        m_subscriberSocket->abort();
    }

    clearPendingCommands(QStringLiteral("Disconnected by client"));
    setConnectionState(Disconnected);
}

RedisGateway::ConnectionState RedisGateway::getConnectionState() const
{
    return m_connectionState;
}

void RedisGateway::subscribe(const QString& channel)
{
    if (channel.isEmpty()) {
        return;
    }

    if (m_subscribedChannels.contains(channel)) {
        return;
    }

    m_subscribedChannels.insert(channel);
    if (isSocketConnected(m_subscriberSocket)) {
        sendCommand(m_subscriberSocket, {QByteArrayLiteral("SUBSCRIBE"), channel.toUtf8()});
    }
}

void RedisGateway::unsubscribe(const QString& channel)
{
    if (channel.isEmpty()) {
        return;
    }

    m_subscribedChannels.remove(channel);
    if (isSocketConnected(m_subscriberSocket)) {
        sendCommand(m_subscriberSocket, {QByteArrayLiteral("UNSUBSCRIBE"), channel.toUtf8()});
    }
}

void RedisGateway::publish(const QString& channel, const QByteArray& message)
{
    if (!isSocketConnected(m_commandSocket)) {
        emit errorOccurred(QStringLiteral("Redis command socket is not connected"));
        return;
    }

    auto* pending = new PendingCommand;
    pending->kind = CommandKind::Publish;
    pending->key = channel;
    m_pendingCommands.append(pending);
    sendCommand(m_commandSocket, {QByteArrayLiteral("PUBLISH"), channel.toUtf8(), message});
}

QVariant RedisGateway::readKey(const QString& key)
{
    if (!isSocketConnected(m_commandSocket)) {
        emit errorOccurred(QStringLiteral("Redis command socket is not connected"));
        return QVariant();
    }

    auto* pending = new PendingCommand;
    pending->kind = CommandKind::Read;
    pending->key = key;

    QEventLoop loop;
    QTimer timeoutTimer;
    timeoutTimer.setSingleShot(true);
    connect(&timeoutTimer, &QTimer::timeout, &loop, &QEventLoop::quit);
    pending->loop = &loop;
    m_pendingCommands.append(pending);
    sendCommand(m_commandSocket, {QByteArrayLiteral("GET"), key.toUtf8()});

    timeoutTimer.start(2000);
    loop.exec();

    if (m_pendingCommands.removeOne(pending)) {
        emit errorOccurred(QStringLiteral("Timed out reading Redis key '%1' from %2:%3")
                               .arg(key, m_host).arg(m_port));
        delete pending;
        return QVariant();
    }

    const QString error = pending->error;
    const QVariant result = pending->result;
    delete pending;
    if (!error.isEmpty()) {
        emit errorOccurred(error);
    }
    return result;
}

void RedisGateway::asyncReadKey(const QString& key)
{
    if (!isSocketConnected(m_commandSocket)) {
        emit errorOccurred(QStringLiteral("Redis command socket is not connected"));
        return;
    }

    auto* pending = new PendingCommand;
    pending->kind = CommandKind::AsyncRead;
    pending->key = key;
    m_pendingCommands.append(pending);
    sendCommand(m_commandSocket, {QByteArrayLiteral("GET"), key.toUtf8()});
}

QString RedisGateway::readString(const QString& key)
{
    return readKey(key).toString();
}

QVariantMap RedisGateway::readJson(const QString& key)
{
    QVariant val = readKey(key);
    if (val.typeId() == QMetaType::QByteArray || val.typeId() == QMetaType::QString) {
        QJsonDocument doc = QJsonDocument::fromJson(val.toByteArray());
        if (doc.isObject()) {
            return doc.object().toVariantMap();
        }
    }
    return val.toMap();
}

void RedisGateway::onCommandSocketConnected()
{
    if (isSocketConnected(m_commandSocket) && isSocketConnected(m_subscriberSocket)) {
        if (m_reconnectTimer->isActive()) {
            m_reconnectTimer->stop();
        }
        setConnectionState(Connected);
    } else {
        setConnectionState(Reconnecting);
    }
}

void RedisGateway::onCommandSocketDisconnected()
{
    if (m_manualDisconnect) {
        return;
    }
    scheduleReconnect(QStringLiteral("Redis command socket disconnected"));
}

void RedisGateway::onCommandSocketReadyRead()
{
    if (!m_commandSocket) {
        return;
    }

    m_commandBuffer.append(m_commandSocket->readAll());
    processCommandBuffer();
}

void RedisGateway::onCommandSocketError()
{
    if (!m_commandSocket) {
        return;
    }

    const QString message = m_commandSocket->errorString();
    emit errorOccurred(QStringLiteral("Redis command socket error: %1").arg(message));
    if (!m_manualDisconnect) {
        scheduleReconnect(message);
    }
}

void RedisGateway::onSubscriberSocketConnected()
{
    for (const QString& channel : m_subscribedChannels) {
        sendCommand(m_subscriberSocket, {QByteArrayLiteral("SUBSCRIBE"), channel.toUtf8()});
    }

    if (isSocketConnected(m_commandSocket) && isSocketConnected(m_subscriberSocket)) {
        if (m_reconnectTimer->isActive()) {
            m_reconnectTimer->stop();
        }
        setConnectionState(Connected);
    } else {
        setConnectionState(Reconnecting);
    }
}

void RedisGateway::onSubscriberSocketDisconnected()
{
    if (m_manualDisconnect) {
        return;
    }
    scheduleReconnect(QStringLiteral("Redis subscriber socket disconnected"));
}

void RedisGateway::onSubscriberSocketReadyRead()
{
    if (!m_subscriberSocket) {
        return;
    }

    m_subscriberBuffer.append(m_subscriberSocket->readAll());
    processSubscriberBuffer();
}

void RedisGateway::onSubscriberSocketError()
{
    if (!m_subscriberSocket) {
        return;
    }

    const QString message = m_subscriberSocket->errorString();
    emit errorOccurred(QStringLiteral("Redis subscriber socket error: %1").arg(message));
    if (!m_manualDisconnect) {
        scheduleReconnect(message);
    }
}

void RedisGateway::onReconnectTimeout()
{
    if (m_manualDisconnect || m_host.isEmpty() || m_port <= 0) {
        return;
    }

    connectSockets();
}

void RedisGateway::ensureSockets()
{
    if (!m_commandSocket) {
        m_commandSocket = new QTcpSocket(this);
        connect(m_commandSocket, &QTcpSocket::connected,
                this, &RedisGateway::onCommandSocketConnected);
        connect(m_commandSocket, &QTcpSocket::disconnected,
                this, &RedisGateway::onCommandSocketDisconnected);
        connect(m_commandSocket, &QTcpSocket::readyRead,
                this, &RedisGateway::onCommandSocketReadyRead);
        connect(m_commandSocket, &QAbstractSocket::errorOccurred,
                this, &RedisGateway::onCommandSocketError);
    }

    if (!m_subscriberSocket) {
        m_subscriberSocket = new QTcpSocket(this);
        connect(m_subscriberSocket, &QTcpSocket::connected,
                this, &RedisGateway::onSubscriberSocketConnected);
        connect(m_subscriberSocket, &QTcpSocket::disconnected,
                this, &RedisGateway::onSubscriberSocketDisconnected);
        connect(m_subscriberSocket, &QTcpSocket::readyRead,
                this, &RedisGateway::onSubscriberSocketReadyRead);
        connect(m_subscriberSocket, &QAbstractSocket::errorOccurred,
                this, &RedisGateway::onSubscriberSocketError);
    }
}

void RedisGateway::setConnectionState(ConnectionState state)
{
    if (m_connectionState == state) {
        return;
    }

    m_connectionState = state;
    emit connectionStateChanged(m_connectionState);
}

void RedisGateway::connectSockets()
{
    if (m_host.isEmpty() || m_port <= 0) {
        emit errorOccurred(QStringLiteral("Redis host or port is not configured"));
        return;
    }

    ensureSockets();
    setConnectionState(Reconnecting);

    if (m_commandSocket->state() == QAbstractSocket::UnconnectedState) {
        m_commandSocket->connectToHost(m_host, static_cast<quint16>(m_port));
    }

    if (m_subscriberSocket->state() == QAbstractSocket::UnconnectedState) {
        m_subscriberSocket->connectToHost(m_host, static_cast<quint16>(m_port));
    }
}

void RedisGateway::scheduleReconnect(const QString& reason)
{
    clearPendingCommands(QStringLiteral("Redis connection dropped: %1").arg(reason));
    setConnectionState(Reconnecting);
    if (!m_reconnectTimer->isActive()) {
        m_reconnectTimer->start();
    }
}

void RedisGateway::clearPendingCommands(const QString& reason)
{
    while (!m_pendingCommands.isEmpty()) {
        PendingCommand* pending = m_pendingCommands.takeFirst();
        pending->error = reason;
        if (pending->kind == CommandKind::AsyncRead) {
            emit errorOccurred(reason);
        }
        if (pending->loop) {
            pending->loop->quit();
        } else {
            delete pending;
        }
    }
}

void RedisGateway::sendCommand(QTcpSocket* socket, const QList<QByteArray>& arguments)
{
    if (!socket || arguments.isEmpty()) {
        return;
    }

    socket->write(encodeRespCommand(arguments));
    socket->flush();
}

void RedisGateway::processCommandBuffer()
{
    while (!m_pendingCommands.isEmpty()) {
        RespValue value;
        int consumed = 0;
        if (!parseRespValue(m_commandBuffer, 0, value, consumed)) {
            return;
        }

        m_commandBuffer.remove(0, consumed);
        PendingCommand* pending = m_pendingCommands.takeFirst();
        if (value.type == RespValue::Type::Error) {
            pending->error = QStringLiteral("Redis command failed: %1")
                                 .arg(QString::fromUtf8(value.stringValue));
        } else if (pending->kind == CommandKind::Read ||
                   pending->kind == CommandKind::AsyncRead) {
            pending->result = respToVariant(value);
        }

        if (pending->kind == CommandKind::AsyncRead) {
            if (!pending->error.isEmpty()) {
                emit errorOccurred(pending->error);
            } else {
                emit keyValueReceived(pending->key, pending->result);
            }
            delete pending;
            continue;
        }

        if (pending->loop) {
            pending->loop->quit();
        } else {
            if (!pending->error.isEmpty()) {
                emit errorOccurred(pending->error);
            }
            delete pending;
        }
    }
}

void RedisGateway::processSubscriberBuffer()
{
    while (true) {
        RespValue value;
        int consumed = 0;
        if (!parseRespValue(m_subscriberBuffer, 0, value, consumed)) {
            return;
        }

        m_subscriberBuffer.remove(0, consumed);
        if (value.type != RespValue::Type::Array || value.arrayValue.isEmpty()) {
            continue;
        }

        const QString messageType = QString::fromUtf8(value.arrayValue.first().stringValue).toLower();
        if (messageType == QStringLiteral("message") && value.arrayValue.size() >= 3) {
            const QString channel = QString::fromUtf8(value.arrayValue.at(1).stringValue);
            const QByteArray payload = value.arrayValue.at(2).stringValue;
            emit messageReceived(channel, payload);
        } else if (messageType == QStringLiteral("subscribe") ||
                   messageType == QStringLiteral("unsubscribe")) {
            continue;
        } else if (messageType == QStringLiteral("pong")) {
            emit messageReceived(QStringLiteral("__redis_internal__"), value.arrayValue.value(1).stringValue);
        }
    }
}
