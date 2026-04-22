#include "RedisSoftwareResolver.h"
#include "communication/hub/IRedisCommandAccess.h"

RedisSoftwareResolver::RedisSoftwareResolver(IRedisCommandAccess* commandAccess, QObject* parent)
    : QObject(parent)
    , m_commandAccess(commandAccess)
{
}

QString RedisSoftwareResolver::resolveSoftwareType()
{
    if (!m_commandAccess) {
        return QStringLiteral("default");
    }

    const QString type = m_commandAccess->readStringValue(QStringLiteral("current_software_type"));
    return type.isEmpty() ? QStringLiteral("default") : type;
}

QVariantMap RedisSoftwareResolver::resolveSoftwareProfile()
{
    if (!m_commandAccess) {
        return {};
    }

    return m_commandAccess->readJsonValue(QStringLiteral("software_profile"));
}
