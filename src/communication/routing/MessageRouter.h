#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QVariantMap>
#include <QByteArray>
#include <QTimer>

class MessageRouter : public QObject
{
    Q_OBJECT

public:
    enum MessageCategory {
        ActionRequest,
        LogicNotify,
        Heartbeat,
        ResyncRequest,
        ResyncResponse,
        ServerCommand
    };
    Q_ENUM(MessageCategory)

    explicit MessageRouter(QObject* parent = nullptr);
    ~MessageRouter() override = default;

    void routeIncomingMessage(const QByteArray& rawMessage);
    void clearExpiredDedup();

signals:
    void ackReceived(const QVariantMap& payload);
    void actionRequestRouted(const QString& module, const QVariantMap& payload);
    void serverCommandRouted(const QString& commandType, const QVariantMap& payload);
    void heartbeatReceived(const QVariantMap& payload);
    void resyncRequestReceived(const QVariantMap& payload);
    void resyncResponseReceived(const QVariantMap& payload);
    void routingError(const QString& errorMessage);

private:
    QMap<QString, qint64> m_deduplicationWindow;
    qint64 m_deduplicationTimeoutMs = 30000;
    QTimer* m_cleanupTimer = nullptr;
};
