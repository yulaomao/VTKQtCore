#pragma once

#include <QObject>
#include <QString>

#include "StateSample.h"

class SourceBase : public QObject
{
    Q_OBJECT

public:
    explicit SourceBase(const QString& sourceId, QObject* parent = nullptr);
    ~SourceBase() override = default;

    QString getSourceId() const;

    virtual void start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;

signals:
    void sampleReady(const StateSample& sample);
    void sourceError(const QString& sourceId, const QString& errorMessage);

protected:
    QString m_sourceId;
};
