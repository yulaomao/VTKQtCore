#pragma once

#include "logic/runtime/GlobalPollingSampleParser.h"
#include "communication/datasource/GlobalPollingPlan.h"

#include <QMap>
#include <QString>

// Config-driven parser: driven entirely by the PollingKeyRoute table set via
// setKeyRoutes() (or taken from a GlobalPollingPlan).  For every poll batch
// it groups all Redis values by module, normalises each value from JSON bytes,
// and emits exactly ONE StateSample per module whose data map contains:
//
//   "values" -> QVariantMap  { subKey: decodedValue, ... }
//   "sourceBatchSampleId" -> QString
//
// This means module handlers never need to inspect raw Redis key names or DB
// numbers – they only see their own labelled sub-keys.
class DefaultGlobalPollingSampleParser final : public GlobalPollingSampleParser
{
public:
    explicit DefaultGlobalPollingSampleParser(QObject* parent = nullptr);

    // Replace the entire route table.
    void setKeyRoutes(const QMap<QString, PollingKeyRoute>& routes);

    QVector<StateSample> parse(const StateSample& batchSample) const override;

private:
    QMap<QString, PollingKeyRoute> m_keyRoutes;
};