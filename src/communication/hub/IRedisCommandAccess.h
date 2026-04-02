#pragma once

#include <QByteArray>
#include <QString>
#include <QVariant>
#include <QVariantMap>

class IRedisCommandAccess
{
public:
    virtual ~IRedisCommandAccess() = default;

    virtual bool isAvailable() const = 0;
    virtual QVariant readValue(const QString& key) = 0;
    virtual QString readStringValue(const QString& key) = 0;
    virtual QVariantMap readJsonValue(const QString& key) = 0;
    virtual bool writeValue(const QString& key, const QVariant& value) = 0;
    virtual bool writeJsonValue(const QString& key, const QVariantMap& value) = 0;
    virtual bool publishMessage(const QString& channel, const QByteArray& message) = 0;
    virtual bool publishJsonMessage(const QString& channel, const QVariantMap& payload) = 0;
};