#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class IRedisCommandAccess;

class RedisSoftwareResolver : public QObject
{
    Q_OBJECT

public:
    explicit RedisSoftwareResolver(IRedisCommandAccess* commandAccess, QObject* parent = nullptr);

    QString resolveSoftwareType();
    QVariantMap resolveSoftwareProfile();

private:
    IRedisCommandAccess* m_commandAccess;
};
