#include "RedisSoftwareResolver.h"
#include "RedisGateway.h"

RedisSoftwareResolver::RedisSoftwareResolver(RedisGateway* gateway, QObject* parent)
    : QObject(parent)
    , gateway(gateway)
{
}

QString RedisSoftwareResolver::resolveSoftwareType()
{
    if (!gateway) {
        return QStringLiteral("default");
    }

    QString type = gateway->readString(QStringLiteral("current_software_type"));
    if (type.isEmpty()) {
        return QStringLiteral("default");
    }
    return type;
}

QVariantMap RedisSoftwareResolver::resolveSoftwareProfile()
{
    if (!gateway) {
        return {};
    }

    QVariantMap profile = gateway->readJson(QStringLiteral("software_profile"));
    return profile;
}
