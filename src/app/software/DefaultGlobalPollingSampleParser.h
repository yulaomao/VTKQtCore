#pragma once

#include "logic/runtime/GlobalPollingSampleParser.h"

class DefaultGlobalPollingSampleParser final : public GlobalPollingSampleParser
{
public:
    explicit DefaultGlobalPollingSampleParser(QObject* parent = nullptr);

    QVector<StateSample> parse(const StateSample& batchSample) const override;
};