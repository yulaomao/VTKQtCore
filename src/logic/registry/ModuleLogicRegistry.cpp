#include "ModuleLogicRegistry.h"
#include "ModuleLogicHandler.h"

ModuleLogicRegistry::ModuleLogicRegistry(QObject* parent)
    : QObject(parent)
{
}

void ModuleLogicRegistry::registerHandler(ModuleLogicHandler* handler)
{
    if (!handler) {
        return;
    }
    handler->setParent(this);
    m_handlers.insert(handler->getModuleId(), handler);
}

void ModuleLogicRegistry::unregisterHandler(const QString& moduleId)
{
    m_handlers.remove(moduleId);
}

ModuleLogicHandler* ModuleLogicRegistry::getHandler(const QString& moduleId) const
{
    return m_handlers.value(moduleId, nullptr);
}

QStringList ModuleLogicRegistry::getRegisteredModules() const
{
    return m_handlers.keys();
}
