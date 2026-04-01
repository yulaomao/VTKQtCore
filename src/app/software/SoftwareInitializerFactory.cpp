#include "SoftwareInitializerFactory.h"
#include "BaseSoftwareInitializer.h"
#include "ConcreteSoftwareInitializers/DefaultSoftwareInitializer.h"

QMap<QString, std::function<BaseSoftwareInitializer*(const QString&, RunMode, QObject*)>>&
SoftwareInitializerFactory::registry()
{
    static QMap<QString, std::function<BaseSoftwareInitializer*(const QString&, RunMode, QObject*)>> s_registry;
    return s_registry;
}

BaseSoftwareInitializer* SoftwareInitializerFactory::create(const QString& softwareType, RunMode mode,
                                                            QObject* parent)
{
    auto& reg = registry();
    auto it = reg.find(softwareType);
    if (it != reg.end()) {
        return it.value()(softwareType, mode, parent);
    }

    return new DefaultSoftwareInitializer(softwareType, mode, parent);
}

void SoftwareInitializerFactory::registerInitializer(const QString& softwareType,
    std::function<BaseSoftwareInitializer*(const QString&, RunMode, QObject*)> creator)
{
    registry().insert(softwareType, std::move(creator));
}
