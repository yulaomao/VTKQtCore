#include "WorkflowStateMachine.h"

WorkflowStateMachine::WorkflowStateMachine(QObject* parent)
    : QObject(parent)
{
}

void WorkflowStateMachine::setWorkflowSequence(const QStringList& sequence)
{
    m_workflowSequence = sequence;
}

QStringList WorkflowStateMachine::getWorkflowSequence() const
{
    return m_workflowSequence;
}

void WorkflowStateMachine::setInitialModule(const QString& module)
{
    m_initialModule = module;
}

QString WorkflowStateMachine::getInitialModule() const
{
    return m_initialModule;
}

void WorkflowStateMachine::setCurrentModule(const QString& module)
{
    const QString oldModule = m_currentModule;
    if (oldModule != module) {
        m_currentModule = module;
        emit currentModuleChanged(module, oldModule);
    }
}

QString WorkflowStateMachine::getCurrentModule() const
{
    return m_currentModule;
}

void WorkflowStateMachine::setEnterableModules(const QSet<QString>& modules)
{
    m_enterableModules = modules;
    emit enterableModulesChanged();
}

QSet<QString> WorkflowStateMachine::getEnterableModules() const
{
    return m_enterableModules;
}

void WorkflowStateMachine::addEnterableModule(const QString& module)
{
    if (!m_enterableModules.contains(module)) {
        m_enterableModules.insert(module);
        emit enterableModulesChanged();
    }
}

void WorkflowStateMachine::removeEnterableModule(const QString& module)
{
    if (m_enterableModules.remove(module)) {
        emit enterableModulesChanged();
    }
}

bool WorkflowStateMachine::isModuleEnterable(const QString& module) const
{
    return m_enterableModules.contains(module);
}

bool WorkflowStateMachine::canAdvanceToNext() const
{
    const QString next = getNextModule();
    return !next.isEmpty() && m_enterableModules.contains(next);
}

bool WorkflowStateMachine::canGoToPrev() const
{
    const QString prev = getPrevModule();
    return !prev.isEmpty() && m_enterableModules.contains(prev);
}

QString WorkflowStateMachine::getNextModule() const
{
    const int idx = getModuleIndex(m_currentModule);
    if (idx < 0 || idx + 1 >= m_workflowSequence.size()) {
        return QString();
    }
    return m_workflowSequence.at(idx + 1);
}

QString WorkflowStateMachine::getPrevModule() const
{
    const int idx = getModuleIndex(m_currentModule);
    if (idx <= 0) {
        return QString();
    }
    return m_workflowSequence.at(idx - 1);
}

int WorkflowStateMachine::getModuleIndex(const QString& module) const
{
    return m_workflowSequence.indexOf(module);
}
