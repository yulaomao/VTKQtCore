#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantMap>
#include <QByteArray>
#include <QList>
#include <QSet>

class QTcpSocket;
class QTimer;
class QEventLoop;

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
    bool waitForConnected(int timeoutMs = 2000);

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

private slots:
    void onCommandSocketConnected();
    void onCommandSocketDisconnected();
    void onCommandSocketReadyRead();
    void onCommandSocketError();
    void onSubscriberSocketConnected();
    void onSubscriberSocketDisconnected();
    void onSubscriberSocketReadyRead();
    void onSubscriberSocketError();
    void onReconnectTimeout();

private:
    enum class CommandKind {
        Read,
        AsyncRead,
        Publish,
        Generic
    };

    struct PendingCommand {
        CommandKind kind = CommandKind::Generic;
        QString key;
        QVariant result;
        QString error;
        QEventLoop* loop = nullptr;
    };

    void ensureSockets();
    void setConnectionState(ConnectionState state);
    void connectSockets();
    void scheduleReconnect(const QString& reason);
    void clearPendingCommands(const QString& reason);
    void sendCommand(QTcpSocket* socket, const QList<QByteArray>& arguments);
    void processCommandBuffer();
    void processSubscriberBuffer();

    ConnectionState m_connectionState = Disconnected;
    QString m_host;
    int m_port = 0;
    QTcpSocket* m_commandSocket = nullptr;
    QTcpSocket* m_subscriberSocket = nullptr;
    QTimer* m_reconnectTimer = nullptr;
    QByteArray m_commandBuffer;
    QByteArray m_subscriberBuffer;
    QList<PendingCommand*> m_pendingCommands;
    QSet<QString> m_subscribedChannels;
    bool m_manualDisconnect = false;
    int m_reconnectDelayMs = 1000;
};
