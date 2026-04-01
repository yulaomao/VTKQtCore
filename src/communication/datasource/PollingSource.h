#pragma once

#include <QObject>
#include <QVector>
#include <QMap>
#include <QVariant>
#include <QTimer>

#include "SourceBase.h"

class PollingTask;

class PollingSource : public SourceBase
{
    Q_OBJECT

public:
    explicit PollingSource(const QString& sourceId, QObject* parent = nullptr);
    ~PollingSource() override = default;

    void start() override;
    void stop() override;
    bool isRunning() const override;

    void addTask(PollingTask* task);
    void removeTask(const QString& taskId);

signals:
    void pollRequested(const QString& redisKey);

public slots:
    void onPollResult(const QString& redisKey, const QVariant& value);

private slots:
    void onTimerTick();

private:
    void recalculateTimerInterval();

    QVector<PollingTask*> m_pollingTasks;
    QTimer* m_timer = nullptr;
    bool m_running = false;
    QMap<QString, QVariant> m_lastValues;
    QMap<QString, qint64> m_lastDispatchTime;
};
