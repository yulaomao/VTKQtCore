#include "ActiveModuleState.h"

ActiveModuleState::ActiveModuleState(QObject* parent)
    : QObject(parent)
{
}

void ActiveModuleState::setInitialModule(const QString& module)
{
    m_initialModule = module;
}

QString ActiveModuleState::getInitialModule() const
{
    return m_initialModule;
}

void ActiveModuleState::setCurrentModule(const QString& module)
{
    const QString oldModule = m_currentModule;
    if (oldModule != module) {
        m_currentModule = module;
        emit currentModuleChanged(module, oldModule);
    }
}

QString ActiveModuleState::getCurrentModule() const
{
    return m_currentModule;
}

QVariantMap ActiveModuleState::createSnapshot() const
{
    return {
        {QStringLiteral("currentModule"), m_currentModule},
        {QStringLiteral("initialModule"), m_initialModule}
    };
}
