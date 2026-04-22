#include "MessageDispatchCenter.h"

#include "RedisConnectionWorker.h"
#include "logic/registry/ModuleLogicHandler.h"
#include "logic/registry/ModuleLogicRegistry.h"

#include <QByteArray>
#include <QJsonDocument>
#include <QThread>

MessageDispatchCenter::MessageDispatchCenter(ModuleLogicRegistry* registry,
                                             QObject* parent)
    : QObject(parent)
    , m_registry(registry)
{
}

MessageDispatchCenter::~MessageDispatchCenter()
{
    stop();
}

void MessageDispatchCenter::configure(const QVector<RedisConnectionConfig>& configs)
{
    // Tear down any existing workers before reconfiguring.
    stop();

    for (const RedisConnectionConfig& cfg : configs) {
        auto* worker = new RedisConnectionWorker(cfg);
        auto* thread = new QThread(this);

        // Move the worker to its dedicated thread.
        worker->moveToThread(thread);

        // Start the worker when the thread's event loop begins.
        connect(thread, &QThread::started,  worker, &RedisConnectionWorker::start);
        connect(thread, &QThread::finished, worker, &QObject::deleteLater);

        // Wire poll and subscription results to this center (queued connection
        // is implicit because worker lives on a different thread).
        connect(worker, &RedisConnectionWorker::pollKeyResult,
                this,   &MessageDispatchCenter::onPollKeyResult);
        connect(worker, &RedisConnectionWorker::subscriptionMessage,
                this,   &MessageDispatchCenter::onSubscriptionMessage);

        m_workers.append(worker);
        m_threads.append(thread);

        // Build the module → worker mapping.
        for (const RedisKeyGroup& group : cfg.pollingKeyGroups) {
            if (!group.module.isEmpty()) {
                m_workerByModule.insert(group.module, worker);
            }
        }
        for (const RedisSubscriptionChannel& sub : cfg.subscriptionChannels) {
            if (!sub.module.isEmpty()) {
                m_workerByModule.insert(sub.module, worker);
            }
        }
    }

    // Set Redis command access on each registered module handler so handlers
    // can call GET/SET/PUBLISH via the appropriate connection worker.
    if (m_registry) {
        for (auto it = m_workerByModule.cbegin(); it != m_workerByModule.cend(); ++it) {
            ModuleLogicHandler* handler = m_registry->getHandler(it.key());
            if (handler) {
                handler->setRedisCommandAccess(it.value());
            }
        }
    }
}

void MessageDispatchCenter::start()
{
    for (QThread* thread : m_threads) {
        if (!thread->isRunning()) {
            thread->start();
        }
    }
}

void MessageDispatchCenter::stop()
{
    for (int i = 0; i < m_workers.size(); ++i) {
        if (m_threads[i]->isRunning()) {
            // Ask the worker to stop (blocking: waits until the slot returns).
            QMetaObject::invokeMethod(m_workers[i], "stop", Qt::BlockingQueuedConnection);
            m_threads[i]->quit();
            m_threads[i]->wait();
        }
    }

    // Workers are deleted by the finished→deleteLater connection.
    m_workers.clear();
    m_threads.clear();
    m_workerByModule.clear();
}

IRedisCommandAccess* MessageDispatchCenter::commandAccessForModule(
    const QString& module) const
{
    return m_workerByModule.value(module, nullptr);
}

// ─── Dispatch: polling ────────────────────────────────────────────────────────

void MessageDispatchCenter::onPollKeyResult(const QString& connectionId,
                                            const QString& module,
                                            const QString& key,
                                            const QVariant& value)
{
    Q_UNUSED(connectionId)

    if (!m_registry) {
        return;
    }

    ModuleLogicHandler* handler = m_registry->getHandler(module);
    if (!handler) {
        return;
    }

    handler->handlePollData(key, value);
}

// ─── Dispatch: subscription ───────────────────────────────────────────────────

void MessageDispatchCenter::onSubscriptionMessage(const QString& connectionId,
                                                  const QString& module,
                                                  const QString& channel,
                                                  const QByteArray& rawData)
{
    Q_UNUSED(connectionId)

    if (!m_registry) {
        return;
    }

    ModuleLogicHandler* handler = m_registry->getHandler(module);
    if (!handler) {
        return;
    }

    // Decode JSON; pass the raw data as fallback if it's not valid JSON.
    QVariantMap data;
    if (!rawData.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(rawData);
        if (doc.isObject()) {
            data = doc.object().toVariantMap();
        } else {
            data.insert(QStringLiteral("raw"), QString::fromUtf8(rawData));
        }
    }

    handler->handleSubscribeData(channel, data);
}
