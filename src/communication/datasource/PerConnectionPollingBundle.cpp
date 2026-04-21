#include "PerConnectionPollingBundle.h"

#include "communication/datasource/PollingSource.h"
#include "communication/redis/RedisPollingWorker.h"

#include <QMetaObject>
#include <QThread>

PerConnectionPollingBundle::PerConnectionPollingBundle(const QString& connectionId,
                                                       const QString& host, int port, int db,
                                                       QObject* parent)
    : QObject(parent)
    , m_connectionId(connectionId)
    , m_db(db)
    , m_pollingSource(new PollingSource(connectionId))
    , m_pollingWorker(new RedisPollingWorker(host, port))
    , m_pollingThread(new QThread(this))
{
    m_pollingThread->setObjectName(
        QStringLiteral("PollingThread_%1").arg(connectionId));

    m_pollingSource->moveToThread(m_pollingThread);
    m_pollingWorker->moveToThread(m_pollingThread);

    connect(m_pollingThread, &QThread::finished,
            m_pollingSource, &QObject::deleteLater);
    connect(m_pollingThread, &QThread::finished,
            m_pollingWorker, &QObject::deleteLater);

    // Wire the polling pipeline: source requests keys → worker does MGET → source receives results.
    connect(m_pollingSource, &PollingSource::batchPollRequested,
            m_pollingWorker, &RedisPollingWorker::readKeys);
    connect(m_pollingWorker, &RedisPollingWorker::keyValuesReceived,
            m_pollingSource, &PollingSource::onBatchPollResult);

    // Lift the batch result from the polling thread to the caller's thread,
    // tagging it with the connectionId so the parser can attribute it.
    connect(m_pollingSource, &PollingSource::sampleReady,
            this, [this](const StateSample& sample) {
                const QVariantMap values =
                    sample.data.value(QStringLiteral("values")).toMap();
                emit pollResultReady(m_connectionId, values);
            });

    connect(m_pollingSource, &PollingSource::sourceError,
            this, [this](const QString& /*sourceId*/, const QString& errorMessage) {
                emit pollingError(m_connectionId, errorMessage);
            });

    m_pollingThread->start();

    // Ask the worker to SELECT the target DB once it is running on the thread.
    if (m_db != 0) {
        QMetaObject::invokeMethod(m_pollingWorker, "selectDb",
                                  Qt::QueuedConnection,
                                  Q_ARG(int, m_db));
    }
}

PerConnectionPollingBundle::~PerConnectionPollingBundle()
{
    // Ensure the polling source is stopped before tearing down the thread so
    // that no further signals are emitted during destruction.
    if (m_pollingSource && m_pollingThread && m_pollingThread->isRunning()) {
        QMetaObject::invokeMethod(m_pollingSource, "stop",
                                  Qt::BlockingQueuedConnection);
    }

    if (m_pollingThread && m_pollingThread->isRunning()) {
        m_pollingThread->quit();
        m_pollingThread->wait();
    }

    m_running = false;
}

QString PerConnectionPollingBundle::getConnectionId() const
{
    return m_connectionId;
}

int PerConnectionPollingBundle::getDb() const
{
    return m_db;
}

void PerConnectionPollingBundle::configurePlan(const GlobalPollingPlan& plan)
{
    QMetaObject::invokeMethod(m_pollingSource, "configurePlan",
                              Qt::BlockingQueuedConnection,
                              Q_ARG(GlobalPollingPlan, plan));
}

void PerConnectionPollingBundle::start()
{
    if (m_running) {
        return;
    }
    m_running = true;
    QMetaObject::invokeMethod(m_pollingSource, "start", Qt::QueuedConnection);
}

void PerConnectionPollingBundle::stop()
{
    if (!m_running) {
        return;
    }
    m_running = false;
    if (m_pollingSource && m_pollingThread && m_pollingThread->isRunning()) {
        QMetaObject::invokeMethod(m_pollingSource, "stop", Qt::QueuedConnection);
    }
}

bool PerConnectionPollingBundle::isRunning() const
{
    return m_running;
}
