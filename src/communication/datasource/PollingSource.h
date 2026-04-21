#pragma once

#include <QObject>
#include <QMap>
#include <QTimer>

#include "SourceBase.h"
#include "GlobalPollingPlan.h"

class PollingSource : public SourceBase
{
    Q_OBJECT

public:
    explicit PollingSource(const QString& sourceId, QObject* parent = nullptr);
    ~PollingSource() override = default;

    Q_INVOKABLE bool hasPlan() const;
    Q_INVOKABLE int getKeyCount() const;
    bool isRunning() const override;

public slots:
    void start() override;
    void stop() override;
    void configurePlan(const GlobalPollingPlan& plan);
    void clearPlan();

signals:
    void batchPollRequested(const QStringList& redisKeys);

public slots:
    void onBatchPollResult(const QVariantMap& values);

private slots:
    void onTimerTick();

private:
    void scheduleNextTick(qint64 nowMs = -1);

    GlobalPollingPlan m_plan;
    QTimer* m_timer = nullptr;
    bool m_running = false;
    QVariantMap m_lastValues;
    qint64 m_lastDispatchTime = 0;
    qint64 m_nextPollDueTime = 0;
};
