#pragma once

#include <QString>
#include <QVariantMap>

#include "communication/datasource/StateSample.h"

struct LegacySocketEnvelope
{
    QString module;
    QString type;
    QVariantMap payload;
    QString errorCode;
    QString errorMessage;

    bool isValid() const { return errorCode.isEmpty(); }
    bool isGlobalTarget() const;
    bool isHeartbeat() const;
    bool isControlMessage() const;
    bool isServerCommand() const;
    bool isResyncMessage() const;
    QString commandType() const;
    StateSample toStateSample() const;
};

class LegacySocketAdapter
{
public:
    static LegacySocketEnvelope fromEnvelope(const QVariantMap& envelope);
    static QVariantMap toEnvelope(const QString& module,
                                  const QString& type,
                                  const QVariantMap& payload);
};
