#pragma once

#include <QObject>
#include <QMap>
#include <QVector>
#include <QString>
#include <QVariant>
#include <QByteArray>

#include "RedisConnectionConfig.h"
#include "communication/hub/IRedisCommandAccess.h"

class ModuleLogicRegistry;
class RedisConnectionWorker;
class QThread;

// Central message dispatch hub.
//
// Creates one RedisConnectionWorker per RedisConnectionConfig entry.
// Each worker polls its Redis keys with MGET and listens for Pub/Sub
// messages on its own QThread.
//
// Results are routed here (via Qt queued signals) and forwarded to the
// appropriate module logic handler found in the ModuleLogicRegistry.
//
// Two dispatch paths:
//   onPollKeyResult       – called when a polled key value arrives.
//   onSubscriptionMessage – called when a Pub/Sub message arrives.
class MessageDispatchCenter : public QObject
{
    Q_OBJECT

public:
    explicit MessageDispatchCenter(ModuleLogicRegistry* registry,
                                   QObject* parent = nullptr);
    ~MessageDispatchCenter() override;

    // Configure connections from a list of connection configs.
    // Must be called before start().
    void configure(const QVector<RedisConnectionConfig>& configs);

    void start();
    void stop();

    // Returns the IRedisCommandAccess for the connection that owns the given module.
    // Returns nullptr if the module is unknown.
    IRedisCommandAccess* commandAccessForModule(const QString& module) const;

public slots:
    // Called (via queued connection) when a polled key value arrives.
    void onPollKeyResult(const QString& connectionId, const QString& module,
                         const QString& key, const QVariant& value);

    // Called (via queued connection) when a Pub/Sub message arrives.
    void onSubscriptionMessage(const QString& connectionId, const QString& module,
                               const QString& channel, const QByteArray& rawData);

private:
    ModuleLogicRegistry*             m_registry;
    QVector<RedisConnectionWorker*>  m_workers;
    QVector<QThread*>                m_threads;
    // Maps module name → the worker that owns it (for command access).
    QMap<QString, RedisConnectionWorker*> m_workerByModule;
};
