#include "SubscriptionSource.h"

SubscriptionSource::SubscriptionSource(const QString& sourceId, const QString& channel,
                                       const QString& module, QObject* parent)
    : SourceBase(sourceId, parent)
    , m_channel(channel)
    , m_module(module)
{
}

void SubscriptionSource::start()
{
    m_running = true;
}

void SubscriptionSource::stop()
{
    m_running = false;
}

bool SubscriptionSource::isRunning() const
{
    return m_running;
}

QString SubscriptionSource::getChannel() const
{
    return m_channel;
}

void SubscriptionSource::onMessageReceived(const QString& channel, const QVariantMap& data)
{
    if (!m_running || channel != m_channel) {
        return;
    }

    StateSample sample = StateSample::create(m_sourceId, m_module,
                                             QStringLiteral("subscription"), data);
    emit sampleReady(sample);
}
