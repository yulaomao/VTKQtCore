#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

#include "communication/datasource/GlobalPollingPlan.h"

class PollingSource;
class RedisPollingWorker;
class QThread;

// Bundles a PollingSource + RedisPollingWorker + dedicated thread for a single
// Redis connection.  Each bundle polls exactly one DB on one server and emits
// its results together with its connectionId so that downstream components can
// route the data to the correct modules.
class PerConnectionPollingBundle : public QObject
{
    Q_OBJECT

public:
    explicit PerConnectionPollingBundle(const QString& connectionId,
                                        const QString& host, int port, int db,
                                        QObject* parent = nullptr);
    ~PerConnectionPollingBundle() override;

    QString getConnectionId() const;
    int     getDb() const;

    void configurePlan(const GlobalPollingPlan& plan);

    // Start/stop polling.  Safe to call from any thread.
    void start();
    void stop();
    bool isRunning() const;

signals:
    // Emitted on the hub's main thread each time a batch poll completes.
    // 'values' is the raw QVariantMap returned by MGET (key → value).
    void pollResultReady(const QString& connectionId, const QVariantMap& values);

    // Emitted when the underlying polling source reports an error.
    void pollingError(const QString& connectionId, const QString& errorMessage);

private:
    QString            m_connectionId;
    int                m_db = 0;
    PollingSource*     m_pollingSource  = nullptr;
    RedisPollingWorker* m_pollingWorker = nullptr;
    QThread*           m_pollingThread  = nullptr;
    bool               m_running        = false;
};
