#include "RedisGateway.h"

#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

RedisGateway::RedisGateway(QObject* parent)
    : QObject(parent)
{
}

void RedisGateway::connectToServer(const QString& host, int port)
{
    m_host = host;
    m_port = port;
    m_connectionState = Connected;
    qDebug() << "RedisGateway: connected to" << host << ":" << port << "(stub)";
    emit connectionStateChanged(m_connectionState);
}

void RedisGateway::disconnect()
{
    m_connectionState = Disconnected;
    qDebug() << "RedisGateway: disconnected (stub)";
    emit connectionStateChanged(m_connectionState);
}

RedisGateway::ConnectionState RedisGateway::getConnectionState() const
{
    return m_connectionState;
}

void RedisGateway::subscribe(const QString& channel)
{
    qDebug() << "RedisGateway: subscribe to" << channel << "(stub)";
}

void RedisGateway::unsubscribe(const QString& channel)
{
    qDebug() << "RedisGateway: unsubscribe from" << channel << "(stub)";
}

void RedisGateway::publish(const QString& channel, const QByteArray& message)
{
    qDebug() << "RedisGateway: publish to" << channel << "size:" << message.size() << "(stub)";
}

QVariant RedisGateway::readKey(const QString& key)
{
    qDebug() << "RedisGateway: readKey" << key << "(stub)";
    return QVariant();
}

void RedisGateway::asyncReadKey(const QString& key)
{
    qDebug() << "RedisGateway: asyncReadKey" << key << "(stub)";
    emit keyValueReceived(key, QVariant());
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
