#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QByteArray>

class RedisGateway : public QObject
{
    Q_OBJECT

public:
    enum ConnectionState {
        Connected,
        Reconnecting,
        Disconnected
    };
    Q_ENUM(ConnectionState)

    explicit RedisGateway(QObject* parent = nullptr);
    ~RedisGateway() override = default;

    void connectToServer(const QString& host, int port);
    void disconnect();
    ConnectionState getConnectionState() const;

    void subscribe(const QString& channel);
    void unsubscribe(const QString& channel);
    void publish(const QString& channel, const QByteArray& message);

    QVariant readKey(const QString& key);
    void asyncReadKey(const QString& key);

    QString readString(const QString& key);
    QVariantMap readJson(const QString& key);

signals:
    void connectionStateChanged(RedisGateway::ConnectionState state);
    void messageReceived(const QString& channel, const QByteArray& message);
    void keyValueReceived(const QString& key, const QVariant& value);
    void errorOccurred(const QString& errorMessage);

private:
    ConnectionState m_connectionState = Disconnected;
    QString m_host;
    int m_port = 0;
};
