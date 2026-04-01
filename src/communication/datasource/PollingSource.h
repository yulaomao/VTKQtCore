#pragma once

#include <QObject>
#include <QMap>
#include <QTimer>
#include <QVariant>
#include <QVector>

#include "SourceBase.h"

class PollingTask;

class PollingSource : public SourceBase
{
    Q_OBJECT

public:
    explicit PollingSource(const QString& sourceId, QObject* parent = nullptr);
    ~PollingSource() override = default;

    Q_INVOKABLE int getTaskCount() const;
    Q_INVOKABLE int getActiveTaskCount() const;
    bool isRunning() const override;

public slots:
    void start() override;
    void stop() override;
    void addTask(PollingTask* task);
    void removeTask(const QString& taskId);

signals:
    void pollRequested(const QString& redisKey);

public slots:
    void onPollResult(const QString& redisKey, const QVariant& value);

private slots:
    void onTimerTick();

private:
    void scheduleNextTick(qint64 nowMs = -1);

    QVector<PollingTask*> m_pollingTasks;
    QTimer* m_timer = nullptr;
    bool m_running = false;
    QMap<QString, QVariant> m_lastValues;
    QMap<QString, qint64> m_lastDispatchTime;
    QMap<QString, qint64> m_nextPollDueTime;
};
