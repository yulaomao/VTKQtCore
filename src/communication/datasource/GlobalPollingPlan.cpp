#include "GlobalPollingPlan.h"

namespace {

QStringList deduplicateKeys(const QStringList& redisKeys)
{
    QStringList uniqueKeys;
    for (const QString& redisKey : redisKeys) {
        const QString trimmed = redisKey.trimmed();
        if (!trimmed.isEmpty() && !uniqueKeys.contains(trimmed)) {
            uniqueKeys.append(trimmed);
        }
    }
    return uniqueKeys;
}

}

GlobalPollingPlan::GlobalPollingPlan(const QString& planId,
                                     const QStringList& redisKeys,
                                     int pollIntervalMs)
    : m_planId(planId)
    , m_redisKeys(deduplicateKeys(redisKeys))
    , m_pollIntervalMs(pollIntervalMs)
{
}

QString GlobalPollingPlan::getPlanId() const { return m_planId; }
void GlobalPollingPlan::setPlanId(const QString& planId) { m_planId = planId; }

QStringList GlobalPollingPlan::getRedisKeys() const { return m_redisKeys; }
void GlobalPollingPlan::setRedisKeys(const QStringList& redisKeys)
{
    m_redisKeys = deduplicateKeys(redisKeys);
}

int GlobalPollingPlan::getPollIntervalMs() const { return m_pollIntervalMs; }
void GlobalPollingPlan::setPollIntervalMs(int intervalMs) { m_pollIntervalMs = intervalMs; }

bool GlobalPollingPlan::getChangeDetection() const { return m_changeDetection; }
void GlobalPollingPlan::setChangeDetection(bool changeDetection)
{
    m_changeDetection = changeDetection;
}

double GlobalPollingPlan::getMaxDispatchRateHz() const { return m_maxDispatchRateHz; }
void GlobalPollingPlan::setMaxDispatchRateHz(double rateHz) { m_maxDispatchRateHz = rateHz; }

bool GlobalPollingPlan::isActive() const { return m_active; }
void GlobalPollingPlan::setActive(bool active) { m_active = active; }