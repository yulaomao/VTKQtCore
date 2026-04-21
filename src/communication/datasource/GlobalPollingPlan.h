#pragma once

#include <QMetaType>
#include <QString>
#include <QStringList>

class GlobalPollingPlan
{
public:
    explicit GlobalPollingPlan(const QString& planId = QString(),
                               const QStringList& redisKeys = {},
                               int pollIntervalMs = 100);

    QString getPlanId() const;
    void setPlanId(const QString& planId);

    QStringList getRedisKeys() const;
    void setRedisKeys(const QStringList& redisKeys);

    int getPollIntervalMs() const;
    void setPollIntervalMs(int intervalMs);

    bool getChangeDetection() const;
    void setChangeDetection(bool changeDetection);

    double getMaxDispatchRateHz() const;
    void setMaxDispatchRateHz(double rateHz);

    bool isActive() const;
    void setActive(bool active);

private:
    QString m_planId;
    QStringList m_redisKeys;
    int m_pollIntervalMs = 100;
    bool m_changeDetection = true;
    double m_maxDispatchRateHz = 30.0;
    bool m_active = true;
};

Q_DECLARE_METATYPE(GlobalPollingPlan)