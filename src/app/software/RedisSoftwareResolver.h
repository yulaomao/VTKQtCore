#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class RedisGateway;

class RedisSoftwareResolver : public QObject
{
    Q_OBJECT

public:
    explicit RedisSoftwareResolver(RedisGateway* gateway, QObject* parent = nullptr);

    QString resolveSoftwareType();
    QVariantMap resolveSoftwareProfile();

private:
    RedisGateway* gateway;
};
