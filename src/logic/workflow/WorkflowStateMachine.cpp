#include "WorkflowStateMachine.h"

namespace {

WorkflowDecision allowDecision(const QString& targetModule, const QString& currentModule)
{
    WorkflowDecision decision;
    decision.allowed = true;
    decision.targetModule = targetModule;
    decision.currentModule = currentModule;
    return decision;
}

WorkflowDecision rejectDecision(const QString& reasonCode,
                                const QString& message,
                                const QString& targetModule,
                                const QString& currentModule)
{
    WorkflowDecision decision;
    decision.allowed = false;
    decision.reasonCode = reasonCode;
    decision.message = message;
    decision.targetModule = targetModule;
    decision.currentModule = currentModule;
    return decision;
}

QStringList setToSortedList(const QSet<QString>& modules)
{
    QStringList result;
    for (const QString& moduleId : modules) {
        result.append(moduleId);
    }
    result.sort();
    return result;
}

}

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

bool WorkflowStateMachine::isModuleKnown(const QString& module) const
{
    return m_workflowSequence.contains(module);
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

WorkflowDecision WorkflowStateMachine::evaluateSwitchTo(const QString& module) const
{
    if (module.isEmpty()) {
        return rejectDecision(
            QStringLiteral("target_empty"),
            QStringLiteral("Target module is empty"),
            module,
            m_currentModule);
    }

    if (!isModuleKnown(module)) {
        return rejectDecision(
            QStringLiteral("target_unknown"),
            QStringLiteral("Module '%1' is not part of the current workflow").arg(module),
            module,
            m_currentModule);
    }

    if (module == m_currentModule) {
        return allowDecision(module, m_currentModule);
    }

    if (!isModuleEnterable(module)) {
        return rejectDecision(
            QStringLiteral("target_not_enterable"),
            QStringLiteral("Module '%1' is not enterable").arg(module),
            module,
            m_currentModule);
    }

    return allowDecision(module, m_currentModule);
}

WorkflowDecision WorkflowStateMachine::evaluateAdvanceToNext() const
{
    if (m_currentModule.isEmpty()) {
        return rejectDecision(
            QStringLiteral("current_module_unset"),
            QStringLiteral("Current workflow module is not set"),
            QString(),
            m_currentModule);
    }

    const QString nextModule = getNextModule();
    if (nextModule.isEmpty()) {
        return rejectDecision(
            QStringLiteral("no_next_module"),
            QStringLiteral("Already at the last step in the workflow"),
            QString(),
            m_currentModule);
    }

    return evaluateSwitchTo(nextModule);
}

WorkflowDecision WorkflowStateMachine::evaluateGoToPrev() const
{
    if (m_currentModule.isEmpty()) {
        return rejectDecision(
            QStringLiteral("current_module_unset"),
            QStringLiteral("Current workflow module is not set"),
            QString(),
            m_currentModule);
    }

    const QString prevModule = getPrevModule();
    if (prevModule.isEmpty()) {
        return rejectDecision(
            QStringLiteral("no_prev_module"),
            QStringLiteral("Already at the first step in the workflow"),
            QString(),
            m_currentModule);
    }

    return evaluateSwitchTo(prevModule);
}

QVariantMap WorkflowStateMachine::createSnapshot() const
{
    return {
        {QStringLiteral("currentModule"), m_currentModule},
        {QStringLiteral("initialModule"), m_initialModule},
        {QStringLiteral("workflowSequence"), m_workflowSequence},
        {QStringLiteral("enterableModules"), setToSortedList(m_enterableModules)}
    };
}
