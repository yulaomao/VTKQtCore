#pragma once

#include "RedisDispatchConfig.h"

#include <QByteArray>
#include <QString>

// Loads a RedisDispatchConfig from a JSON file or raw JSON bytes.
//
// On any parse or validation error a warning is printed via qWarning() and an
// empty (invalid) config is returned so that callers can fall back gracefully.
class RedisDispatchConfigLoader
{
public:
    // Load from a file path.  Qt resource paths (":/…") are supported.
    static RedisDispatchConfig loadFromFile(const QString& filePath);

    // Load from an already-read JSON byte array.
    static RedisDispatchConfig loadFromJson(const QByteArray& json);
};
