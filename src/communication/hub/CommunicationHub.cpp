#include "CommunicationHub.h"

#include "communication/redis/RedisGateway.h"
#include "communication/routing/MessageRouter.h"
#include "communication/datasource/SubscriptionSource.h"
#include "communication/datasource/PollingSource.h"
#include "communication/datasource/PollingTask.h"

#include <QJsonDocument>
#include <QJsonObject>

CommunicationHub::CommunicationHub(RedisGateway* gateway, QObject* parent)
    : QObject(parent)
    , m_redisGateway(gateway)
    , m_messageRouter(new MessageRouter(this))
    , m_pollingSource(new PollingSource(QStringLiteral("polling_source"), this))
{
}

void CommunicationHub::initialize()
{
    connect(m_redisGateway, &RedisGateway::messageReceived,
            this, &CommunicationHub::onGatewayMessageReceived);

    connect(m_messageRouter, &MessageRouter::actionRequestRouted,
            this, &CommunicationHub::controlMessageReceived);
    connect(m_messageRouter, &MessageRouter::serverCommandRouted,
            this, &CommunicationHub::serverCommandReceived);
    connect(m_messageRouter, &MessageRouter::heartbeatReceived,
            this, &CommunicationHub::heartbeatReceived);

    connect(m_pollingSource, &PollingSource::sampleReady,
            this, &CommunicationHub::stateSampleReceived);
    connect(m_pollingSource, &PollingSource::sourceError,
            this, &CommunicationHub::communicationError);

    connect(m_pollingSource, &PollingSource::pollRequested,
            m_redisGateway, &RedisGateway::asyncReadKey);
    connect(m_redisGateway, &RedisGateway::keyValueReceived,
            m_pollingSource, &PollingSource::onPollResult);

    connect(m_redisGateway, &RedisGateway::errorOccurred,
            this, [this](const QString& errorMessage) {
                emit communicationError(QStringLiteral("RedisGateway"), errorMessage);
            });
}

void CommunicationHub::addSubscriptionSource(SubscriptionSource* source)
{
    source->setParent(this);
    m_subscriptionSources.append(source);
    connectSource(source);
}

void CommunicationHub::addPollingTask(PollingTask* task)
{
    m_pollingSource->addTask(task);
}

void CommunicationHub::start()
{
    for (SubscriptionSource* source : m_subscriptionSources) {
        source->start();
        m_redisGateway->subscribe(source->getChannel());
    }
    m_pollingSource->start();
}

void CommunicationHub::stop()
{
    m_pollingSource->stop();
    for (SubscriptionSource* source : m_subscriptionSources) {
        source->stop();
        m_redisGateway->unsubscribe(source->getChannel());
    }
}

void CommunicationHub::onGatewayMessageReceived(const QString& channel, const QByteArray& message)
{
    m_messageRouter->routeIncomingMessage(message);

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(message, &parseError);
    QVariantMap data;
    if (parseError.error == QJsonParseError::NoError && doc.isObject()) {
        data = doc.object().toVariantMap();
    }

    for (SubscriptionSource* source : m_subscriptionSources) {
        source->onMessageReceived(channel, data);
    }
}

void CommunicationHub::connectSource(SubscriptionSource* source)
{
    connect(source, &SubscriptionSource::sampleReady,
            this, &CommunicationHub::stateSampleReceived);
    connect(source, &SubscriptionSource::sourceError,
            this, &CommunicationHub::communicationError);
}
