#include "PollingSource.h"

#include <QDateTime>

namespace {
constexpr int MIN_TIMER_INTERVAL_MS = 10;
constexpr auto kGlobalPollSampleType = "global_poll_batch";

qint64 currentTimeMs()
{
    return QDateTime::currentMSecsSinceEpoch();
}
}

PollingSource::PollingSource(const QString& sourceId, QObject* parent)
    : SourceBase(sourceId, parent)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &PollingSource::onTimerTick);
}

void PollingSource::start()
{
    if (m_running) {
        return;
    }

    m_running = true;
    if (m_plan.isActive() && !m_plan.getRedisKeys().isEmpty()) {
        m_nextPollDueTime = currentTimeMs();
    }
    scheduleNextTick(m_nextPollDueTime);
}

void PollingSource::stop()
{
    if (!m_running) {
        return;
    }

    m_running = false;
    m_timer->stop();
    m_nextPollDueTime = 0;
}

bool PollingSource::isRunning() const
{
    return m_running;
}

bool PollingSource::hasPlan() const
{
    return !m_plan.getPlanId().isEmpty();
}

int PollingSource::getKeyCount() const
{
    return m_plan.getRedisKeys().size();
}

void PollingSource::configurePlan(const GlobalPollingPlan& plan)
{
    m_plan = plan;
    m_lastValues.clear();
    m_lastDispatchTime = 0;
    if (m_running && m_plan.isActive() && !m_plan.getRedisKeys().isEmpty()) {
        m_nextPollDueTime = currentTimeMs();
        scheduleNextTick();
    } else if (!m_plan.isActive() || m_plan.getRedisKeys().isEmpty()) {
        m_nextPollDueTime = 0;
        m_timer->stop();
    }
}

void PollingSource::clearPlan()
{
    configurePlan(GlobalPollingPlan());
}

void PollingSource::onBatchPollResult(const QVariantMap& values)
{
    if (!m_running || !m_plan.isActive() || m_plan.getRedisKeys().isEmpty()) {
        return;
    }

    const qint64 now = currentTimeMs();
    if (m_plan.getChangeDetection() && !m_lastValues.isEmpty() && m_lastValues == values) {
        return;
    }

    if (m_plan.getMaxDispatchRateHz() > 0.0 && m_lastDispatchTime > 0) {
        const double minIntervalMs = 1000.0 / m_plan.getMaxDispatchRateHz();
        if (now - m_lastDispatchTime < static_cast<qint64>(minIntervalMs)) {
            return;
        }
    }

    m_lastValues = values;
    m_lastDispatchTime = now;

    QVariantMap data;
    data.insert(QStringLiteral("planId"), m_plan.getPlanId());
    data.insert(QStringLiteral("keys"), m_plan.getRedisKeys());
    data.insert(QStringLiteral("values"), values);

    emit sampleReady(StateSample::create(
        m_sourceId,
        QString(),
        QString::fromLatin1(kGlobalPollSampleType),
        data));
}

void PollingSource::onTimerTick()
{
    if (!m_running || !m_plan.isActive() || m_plan.getRedisKeys().isEmpty()) {
        return;
    }

    const qint64 now = currentTimeMs();
    if (m_nextPollDueTime > now) {
        scheduleNextTick(now);
        return;
    }

    emit batchPollRequested(m_plan.getRedisKeys());
    m_nextPollDueTime = now + std::max(MIN_TIMER_INTERVAL_MS, m_plan.getPollIntervalMs());

    scheduleNextTick(now);
}

void PollingSource::scheduleNextTick(qint64 nowMs)
{
    if (!m_running) {
        m_timer->stop();
        return;
    }

    if (!m_plan.isActive() || m_plan.getRedisKeys().isEmpty()) {
        m_timer->stop();
        return;
    }

    const qint64 now = nowMs >= 0 ? nowMs : currentTimeMs();
    if (m_nextPollDueTime <= 0) {
        m_nextPollDueTime = now;
    }

    const int nextIntervalMs = static_cast<int>(std::max<qint64>(0, m_nextPollDueTime - now));
    m_timer->start(nextIntervalMs);
}
