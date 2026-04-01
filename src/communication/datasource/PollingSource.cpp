#include "PollingSource.h"
#include "PollingTask.h"

#include <QDateTime>
#include <algorithm>
#include <limits>

namespace {
constexpr int MIN_TIMER_INTERVAL_MS = 10;

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
    const qint64 now = currentTimeMs();
    for (PollingTask* task : m_pollingTasks) {
        if (task && task->isActive()) {
            m_nextPollDueTime.insert(task->getTaskId(), now);
        }
    }
    scheduleNextTick(now);
}

void PollingSource::stop()
{
    if (!m_running) {
        return;
    }

    m_running = false;
    m_timer->stop();
    m_nextPollDueTime.clear();
}

bool PollingSource::isRunning() const
{
    return m_running;
}

int PollingSource::getTaskCount() const
{
    return m_pollingTasks.size();
}

int PollingSource::getActiveTaskCount() const
{
    int count = 0;
    for (const PollingTask* task : m_pollingTasks) {
        if (task && task->isActive()) {
            ++count;
        }
    }
    return count;
}

void PollingSource::addTask(PollingTask* task)
{
    if (!task) {
        return;
    }

    for (const PollingTask* existing : m_pollingTasks) {
        if (existing && existing->getTaskId() == task->getTaskId()) {
            emit sourceError(m_sourceId,
                             QStringLiteral("Duplicate polling task '%1'").arg(task->getTaskId()));
            return;
        }
    }

    task->setParent(this);
    m_pollingTasks.append(task);
    if (m_running && task->isActive()) {
        m_nextPollDueTime.insert(task->getTaskId(), currentTimeMs());
        scheduleNextTick();
    }
}

void PollingSource::removeTask(const QString& taskId)
{
    for (int i = 0; i < m_pollingTasks.size(); ++i) {
        if (m_pollingTasks[i]->getTaskId() == taskId) {
            PollingTask* task = m_pollingTasks.takeAt(i);
            m_lastValues.remove(task->getTaskId());
            m_lastDispatchTime.remove(task->getTaskId());
            m_nextPollDueTime.remove(task->getTaskId());
            task->deleteLater();
            break;
        }
    }
    scheduleNextTick();
}

void PollingSource::onPollResult(const QString& redisKey, const QVariant& value)
{
    if (!m_running) {
        return;
    }

    bool handled = false;
    for (PollingTask* task : m_pollingTasks) {
        if (!task || !task->isActive() || task->getRedisKey() != redisKey) {
            continue;
        }

        const QString taskId = task->getTaskId();

        if (task->getChangeDetection() && m_lastValues.contains(taskId)) {
            if (m_lastValues.value(taskId) == value) {
                continue;
            }
        }

        const qint64 now = currentTimeMs();
        if (task->getMaxDispatchRateHz() > 0.0) {
            const double minIntervalMs = 1000.0 / task->getMaxDispatchRateHz();
            if (m_lastDispatchTime.contains(taskId)) {
                const qint64 elapsed = now - m_lastDispatchTime.value(taskId);
                if (elapsed < static_cast<qint64>(minIntervalMs)) {
                    continue;
                }
            }
        }

        m_lastValues[taskId] = value;
        m_lastDispatchTime[taskId] = now;

        QVariantMap data;
        data[QStringLiteral("key")] = redisKey;
        data[QStringLiteral("value")] = value;
        data[QStringLiteral("taskId")] = taskId;

        StateSample sample = StateSample::create(
            m_sourceId, task->getModule(),
            task->getRedisKey(), data);
        emit sampleReady(sample);
        handled = true;
    }

    if (!handled) {
        return;
    }
}

void PollingSource::onTimerTick()
{
    if (!m_running) {
        return;
    }

    const qint64 now = currentTimeMs();
    for (PollingTask* task : m_pollingTasks) {
        if (!task || !task->isActive()) {
            continue;
        }

        const QString taskId = task->getTaskId();
        const qint64 dueTime = m_nextPollDueTime.value(taskId, now);
        if (dueTime > now) {
            continue;
        }

        emit pollRequested(task->getRedisKey());
        m_nextPollDueTime.insert(
            taskId,
            now + std::max(MIN_TIMER_INTERVAL_MS, task->getPollIntervalMs()));
    }

    scheduleNextTick(now);
}

void PollingSource::scheduleNextTick(qint64 nowMs)
{
    if (!m_running) {
        m_timer->stop();
        return;
    }

    const qint64 now = nowMs >= 0 ? nowMs : currentTimeMs();
    qint64 earliestDueTime = std::numeric_limits<qint64>::max();
    bool hasActiveTask = false;

    for (PollingTask* task : m_pollingTasks) {
        if (!task || !task->isActive()) {
            continue;
        }

        hasActiveTask = true;
        const QString taskId = task->getTaskId();
        if (!m_nextPollDueTime.contains(taskId)) {
            m_nextPollDueTime.insert(taskId, now);
        }

        earliestDueTime = std::min(earliestDueTime, m_nextPollDueTime.value(taskId));
    }

    if (!hasActiveTask) {
        m_timer->stop();
        return;
    }

    const int nextIntervalMs = static_cast<int>(std::max<qint64>(0, earliestDueTime - now));
    m_timer->start(nextIntervalMs);
}
