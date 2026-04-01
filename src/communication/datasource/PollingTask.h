#pragma once

#include <QObject>
#include <QString>

class PollingTask : public QObject
{
    Q_OBJECT

public:
    explicit PollingTask(const QString& taskId, const QString& module,
                         const QString& redisKey, int pollIntervalMs = 100,
                         QObject* parent = nullptr);
    ~PollingTask() override = default;

    QString getTaskId() const;
    void setTaskId(const QString& taskId);

    QString getModule() const;
    void setModule(const QString& module);

    QString getRedisKey() const;
    void setRedisKey(const QString& redisKey);

    int getPollIntervalMs() const;
    void setPollIntervalMs(int intervalMs);

    int getPriority() const;
    void setPriority(int priority);

    bool getLatestWins() const;
    void setLatestWins(bool latestWins);

    bool getChangeDetection() const;
    void setChangeDetection(bool changeDetection);

    double getMaxDispatchRateHz() const;
    void setMaxDispatchRateHz(double rateHz);

    bool isActive() const;
    void setActive(bool active);

private:
    QString m_taskId;
    QString m_module;
    QString m_redisKey;
    int m_pollIntervalMs = 100;
    int m_priority = 0;
    bool m_latestWins = true;
    bool m_changeDetection = true;
    double m_maxDispatchRateHz = 30.0;
    bool m_active = true;
};

Q_DECLARE_METATYPE(PollingTask*)
