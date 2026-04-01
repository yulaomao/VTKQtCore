#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "contracts/UiAction.h"

struct WorkflowDecision
{
    bool allowed = false;
    QString reasonCode;
    QString message;
    QString targetModule;
    QString currentModule;

    QVariantMap toVariantMap() const
    {
        return {
            {QStringLiteral("allowed"), allowed},
            {QStringLiteral("reasonCode"), reasonCode},
            {QStringLiteral("message"), message},
            {QStringLiteral("targetModule"), targetModule},
            {QStringLiteral("currentModule"), currentModule}
        };
    }
};

class WorkflowStateMachine : public QObject
{
    Q_OBJECT

public:
    explicit WorkflowStateMachine(QObject* parent = nullptr);

    void setWorkflowSequence(const QStringList& sequence);
    QStringList getWorkflowSequence() const;
    void setInitialModule(const QString& module);
    QString getInitialModule() const;

    void setCurrentModule(const QString& module);
    QString getCurrentModule() const;

    void setEnterableModules(const QSet<QString>& modules);
    QSet<QString> getEnterableModules() const;
    void addEnterableModule(const QString& module);
    void removeEnterableModule(const QString& module);
    bool isModuleEnterable(const QString& module) const;
    bool isModuleKnown(const QString& module) const;

    bool canAdvanceToNext() const;
    bool canGoToPrev() const;
    QString getNextModule() const;
    QString getPrevModule() const;

    WorkflowDecision evaluateSwitchTo(const QString& module) const;
    WorkflowDecision evaluateAdvanceToNext() const;
    WorkflowDecision evaluateGoToPrev() const;
    WorkflowDecision evaluateAction(const UiAction& action) const;
    QVariantMap createSnapshot() const;

    int getModuleIndex(const QString& module) const;

signals:
    void currentModuleChanged(const QString& newModule, const QString& oldModule);
    void enterableModulesChanged();

private:
    QStringList m_workflowSequence;
    QString m_currentModule;
    QSet<QString> m_enterableModules;
    QString m_initialModule;
};
