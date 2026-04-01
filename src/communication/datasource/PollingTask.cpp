#include "PollingTask.h"

PollingTask::PollingTask(const QString& taskId, const QString& module,
                         const QString& redisKey, int pollIntervalMs,
                         QObject* parent)
    : QObject(parent)
    , m_taskId(taskId)
    , m_module(module)
    , m_redisKey(redisKey)
    , m_pollIntervalMs(pollIntervalMs)
{
}

QString PollingTask::getTaskId() const { return m_taskId; }
void PollingTask::setTaskId(const QString& taskId) { m_taskId = taskId; }

QString PollingTask::getModule() const { return m_module; }
void PollingTask::setModule(const QString& module) { m_module = module; }

QString PollingTask::getRedisKey() const { return m_redisKey; }
void PollingTask::setRedisKey(const QString& redisKey) { m_redisKey = redisKey; }

int PollingTask::getPollIntervalMs() const { return m_pollIntervalMs; }
void PollingTask::setPollIntervalMs(int intervalMs) { m_pollIntervalMs = intervalMs; }

int PollingTask::getPriority() const { return m_priority; }
void PollingTask::setPriority(int priority) { m_priority = priority; }

bool PollingTask::getLatestWins() const { return m_latestWins; }
void PollingTask::setLatestWins(bool latestWins) { m_latestWins = latestWins; }

bool PollingTask::getChangeDetection() const { return m_changeDetection; }
void PollingTask::setChangeDetection(bool changeDetection) { m_changeDetection = changeDetection; }

double PollingTask::getMaxDispatchRateHz() const { return m_maxDispatchRateHz; }
void PollingTask::setMaxDispatchRateHz(double rateHz) { m_maxDispatchRateHz = rateHz; }

bool PollingTask::isActive() const { return m_active; }
void PollingTask::setActive(bool active) { m_active = active; }
