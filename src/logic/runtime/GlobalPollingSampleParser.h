#pragma once

#include <QObject>
#include <QVector>

#include "communication/datasource/StateSample.h"

class GlobalPollingSampleParser : public QObject
{
public:
    explicit GlobalPollingSampleParser(QObject* parent = nullptr)
        : QObject(parent)
    {
    }

    ~GlobalPollingSampleParser() override = default;

    virtual QVector<StateSample> parse(const StateSample& batchSample) const = 0;
};