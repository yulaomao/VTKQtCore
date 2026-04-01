#pragma once

#include <QObject>
#include <QVector>

#include "communication/datasource/StateSample.h"

class RedisGateway;
class MessageRouter;
class SubscriptionSource;
class PollingSource;
class PollingTask;

class CommunicationHub : public QObject
{
    Q_OBJECT

public:
    explicit CommunicationHub(RedisGateway* gateway, QObject* parent = nullptr);
    ~CommunicationHub() override = default;

    void initialize();
    void addSubscriptionSource(SubscriptionSource* source);
    void addPollingTask(PollingTask* task);
    void start();
    void stop();

signals:
    void controlMessageReceived(const QString& module, const QVariantMap& payload);
    void serverCommandReceived(const QString& commandType, const QVariantMap& payload);
    void stateSampleReceived(const StateSample& sample);
    void communicationError(const QString& source, const QString& errorMessage);
    void heartbeatReceived();

private slots:
    void onGatewayMessageReceived(const QString& channel, const QByteArray& message);

private:
    void connectSource(SubscriptionSource* source);

    RedisGateway* m_redisGateway = nullptr;
    MessageRouter* m_messageRouter = nullptr;
    QVector<SubscriptionSource*> m_subscriptionSources;
    PollingSource* m_pollingSource = nullptr;
};
