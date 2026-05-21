#include "ModuleLogicRegistry.h"
#include "ModuleLogicHandler.h"

namespace {

QString normalizeRegistryKey(const QString& value)
{
    return value.trimmed().toLower();
}

ModuleRuntimeDescriptor defaultDescriptorFor(ModuleLogicHandler* handler)
{
    ModuleRuntimeDescriptor descriptor;
    if (!handler) {
        return descriptor;
    }

    descriptor.moduleId = handler->getModuleId();
    descriptor.displayName = descriptor.moduleId;
    descriptor.capabilities = {
        QStringLiteral("ui_intent"),
        QStringLiteral("external_data"),
        QStringLiteral("module_event")
    };
    return descriptor;
}

} // namespace

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
    const QString moduleId = handler->getModuleId().trimmed();
    if (moduleId.isEmpty()) {
        return;
    }

    m_handlers.insert(moduleId, handler);
    if (!m_descriptors.contains(moduleId)) {
        registerDescriptor(defaultDescriptorFor(handler));
    }
}

void ModuleLogicRegistry::unregisterHandler(const QString& moduleId)
{
    const QString resolvedModuleId = resolveModuleId(moduleId);
    if (resolvedModuleId.isEmpty()) {
        return;
    }

    const ModuleRuntimeDescriptor descriptor = m_descriptors.value(resolvedModuleId);
    for (const QString& alias : descriptor.aliases) {
        m_aliasToModuleId.remove(normalizeRegistryKey(alias));
    }
    m_handlers.remove(resolvedModuleId);
    m_descriptors.remove(resolvedModuleId);
}

ModuleLogicHandler* ModuleLogicRegistry::getHandler(const QString& moduleId) const
{
    const QString resolvedModuleId = resolveModuleId(moduleId);
    return resolvedModuleId.isEmpty() ? nullptr : m_handlers.value(resolvedModuleId, nullptr);
}

QStringList ModuleLogicRegistry::getRegisteredModules() const
{
    return m_handlers.keys();
}

void ModuleLogicRegistry::registerDescriptor(const ModuleRuntimeDescriptor& descriptor)
{
    ModuleRuntimeDescriptor normalized = descriptor;
    normalized.moduleId = normalized.moduleId.trimmed();
    if (normalized.moduleId.isEmpty()) {
        return;
    }
    if (normalized.displayName.trimmed().isEmpty()) {
        normalized.displayName = normalized.moduleId;
    }

    const ModuleRuntimeDescriptor previous = m_descriptors.value(normalized.moduleId);
    for (const QString& alias : previous.aliases) {
        m_aliasToModuleId.remove(normalizeRegistryKey(alias));
    }

    m_descriptors.insert(normalized.moduleId, normalized);
    m_aliasToModuleId.insert(normalizeRegistryKey(normalized.moduleId), normalized.moduleId);
    for (const QString& alias : normalized.aliases) {
        const QString normalizedAlias = normalizeRegistryKey(alias);
        if (!normalizedAlias.isEmpty()) {
            m_aliasToModuleId.insert(normalizedAlias, normalized.moduleId);
        }
    }
}

ModuleRuntimeDescriptor ModuleLogicRegistry::descriptorFor(const QString& moduleIdOrAlias) const
{
    const QString resolvedModuleId = resolveModuleId(moduleIdOrAlias);
    return resolvedModuleId.isEmpty()
        ? ModuleRuntimeDescriptor()
        : m_descriptors.value(resolvedModuleId);
}

QString ModuleLogicRegistry::resolveModuleId(const QString& moduleIdOrAlias) const
{
    const QString key = normalizeRegistryKey(moduleIdOrAlias);
    if (key.isEmpty()) {
        return QString();
    }

    const QString resolved = m_aliasToModuleId.value(key);
    if (!resolved.isEmpty()) {
        return resolved;
    }

    return m_handlers.contains(moduleIdOrAlias) || m_descriptors.contains(moduleIdOrAlias)
        ? moduleIdOrAlias
        : QString();
}

bool ModuleLogicRegistry::contains(const QString& moduleIdOrAlias) const
{
    return !resolveModuleId(moduleIdOrAlias).isEmpty();
}
