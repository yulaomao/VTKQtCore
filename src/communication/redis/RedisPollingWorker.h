#pragma once

#include <QObject>
#include <QString>
#include <QVariant>

struct redisContext;

// Persistent Redis connection that lives on the polling thread.
// All slot calls execute synchronously on whichever thread this object
// was moved to (CommunicationHubPollingThread).  One TCP connection is
// kept open for the lifetime of the worker; a failed GET triggers a
// reconnect attempt on the next call.
class RedisPollingWorker : public QObject
{
    Q_OBJECT

public:
    explicit RedisPollingWorker(const QString& host, int port,
                                int connectTimeoutMs = 2000,
                                QObject* parent = nullptr);
    ~RedisPollingWorker() override;

public slots:
    void setConnection(const QString& host, int port);
    void setPollingKeys(const QStringList& keys);
    // Switch to the given DB index.  The command is sent immediately if the
    // worker is already connected; otherwise it is applied on the next
    // successful reconnect.
    void selectDb(int db);
    void poll();

signals:
    void keyValuesReceived(const QVariantMap& values);

private:
    bool ensureConnected();
    void closeContext();

    QString m_host;
    int m_port = 0;
    int m_db = 0;
    int m_connectTimeoutMs = 2000;
    QStringList m_pollingKeys;
    redisContext* m_context = nullptr;
};
