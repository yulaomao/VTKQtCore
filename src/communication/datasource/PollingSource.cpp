#include "PollingSource.h"
#include "PollingTask.h"

#include <QDateTime>
#include <algorithm>
#include <limits>

PollingSource::PollingSource(const QString& sourceId, QObject* parent)
    : SourceBase(sourceId, parent)
    , m_timer(new QTimer(this))
{
    connect(m_timer, &QTimer::timeout, this, &PollingSource::onTimerTick);
}

void PollingSource::start()
{
    m_running = true;
    recalculateTimerInterval();
    m_timer->start();
}

void PollingSource::stop()
{
    m_running = false;
    m_timer->stop();
}

bool PollingSource::isRunning() const
{
    return m_running;
}

void PollingSource::addTask(PollingTask* task)
{
    task->setParent(this);
    m_pollingTasks.append(task);
    recalculateTimerInterval();
}

void PollingSource::removeTask(const QString& taskId)
{
    for (int i = 0; i < m_pollingTasks.size(); ++i) {
        if (m_pollingTasks[i]->getTaskId() == taskId) {
            PollingTask* task = m_pollingTasks.takeAt(i);
            m_lastValues.remove(task->getRedisKey());
            m_lastDispatchTime.remove(task->getRedisKey());
            delete task;
            break;
        }
    }
    recalculateTimerInterval();
}

void PollingSource::onPollResult(const QString& redisKey, const QVariant& value)
{
    if (!m_running) {
        return;
    }

    PollingTask* matchedTask = nullptr;
    for (PollingTask* task : m_pollingTasks) {
        if (task->getRedisKey() == redisKey && task->isActive()) {
            matchedTask = task;
            break;
        }
    }

    if (!matchedTask) {
        return;
    }

    if (matchedTask->getChangeDetection() && m_lastValues.contains(redisKey)) {
        if (m_lastValues.value(redisKey) == value) {
            return;
        }
    }

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (matchedTask->getMaxDispatchRateHz() > 0.0) {
        double minIntervalMs = 1000.0 / matchedTask->getMaxDispatchRateHz();
        if (m_lastDispatchTime.contains(redisKey)) {
            qint64 elapsed = now - m_lastDispatchTime.value(redisKey);
            if (elapsed < static_cast<qint64>(minIntervalMs)) {
                return;
            }
        }
    }

    m_lastValues[redisKey] = value;
    m_lastDispatchTime[redisKey] = now;

    QVariantMap data;
    data[QStringLiteral("key")] = redisKey;
    data[QStringLiteral("value")] = value;

    StateSample sample = StateSample::create(
        m_sourceId, matchedTask->getModule(),
        matchedTask->getRedisKey(), data);
    emit sampleReady(sample);
}

void PollingSource::onTimerTick()
{
    for (PollingTask* task : m_pollingTasks) {
        if (task->isActive()) {
            emit pollRequested(task->getRedisKey());
        }
    }
}

void PollingSource::recalculateTimerInterval()
{
    if (m_pollingTasks.isEmpty()) {
        m_timer->setInterval(100);
        return;
    }

    int minInterval = std::numeric_limits<int>::max();
    for (const PollingTask* task : m_pollingTasks) {
        if (task->isActive() && task->getPollIntervalMs() < minInterval) {
            minInterval = task->getPollIntervalMs();
        }
    }

    m_timer->setInterval(std::max(10, minInterval));
}
