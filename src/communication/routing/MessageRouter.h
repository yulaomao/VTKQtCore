#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QVariantMap>
#include <QByteArray>

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
    void actionRequestRouted(const QString& module, const QVariantMap& payload);
    void serverCommandRouted(const QString& commandType, const QVariantMap& payload);
    void heartbeatReceived();
    void resyncRequestReceived(const QVariantMap& payload);
    void resyncResponseReceived(const QVariantMap& payload);

private:
    QMap<QString, qint64> m_deduplicationWindow;
    qint64 m_deduplicationTimeoutMs = 30000;
};
