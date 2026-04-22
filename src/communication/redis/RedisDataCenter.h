#pragma once

#include <QObject>
#include <QVariant>
#include <QVariantMap>
#include <QVector>

#include "RedisConnectionConfig.h"

class RedisConnectionWorker;
class LogicRuntime;

// ---------------------------------------------------------------------------
// RedisDataCenter
//
// The single, simple hub for all Redis data traffic.
//
// It creates one RedisConnectionWorker per entry in the config, connects to
// each worker's signals, and dispatches the results to the application's
// LogicRuntime via two functions:
//
//   onPollBatch()     — called when a MGET round completes for a connection.
//                       Splits the raw key→value map by module (using the
//                       pollingKeyGroups declared in the config), normalises
//                       each value (JSON decode), aggregates per-module maps,
//                       and calls LogicRuntime::onModulePollBatch() once per
//                       module.
//
//   onSubscription()  — called when a pub/sub message arrives.
//                       Calls LogicRuntime::onModuleSubscription() with the
//                       module name and decoded payload.
//
// "global" module
// ---------------
// When a key group or subscription channel carries module == "global", the
// data is delivered to EVERY registered module logic handler.  The handlers
// themselves decide whether the data is relevant to them.
// ---------------------------------------------------------------------------
class RedisDataCenter : public QObject
{
    Q_OBJECT

public:
    explicit RedisDataCenter(const QVector<RedisConnectionConfig>& configs,
                              LogicRuntime* runtime,
                              QObject* parent = nullptr);
    ~RedisDataCenter() override;

    void start();
    void stop();
    bool isRunning() const;

private slots:
    // Called when a MGET batch result arrives from a worker.
    void onPollBatch(const QString& connectionId, const QVariantMap& rawValues);

    // Called when a pub/sub message arrives from a worker.
    void onSubscription(const QString& connectionId,
                         const QString& module,
                         const QString& channel,
                         const QVariantMap& payload);

private:
    // Normalise a raw Redis value (QByteArray / QString) to a proper QVariant.
    // Attempts JSON decoding; falls back to a plain string.
    static QVariant normalizeValue(const QVariant& raw);

    QVector<RedisConnectionConfig>  m_configs;
    QVector<RedisConnectionWorker*> m_workers;
    LogicRuntime*                    m_runtime = nullptr;
    bool                             m_running  = false;
};
