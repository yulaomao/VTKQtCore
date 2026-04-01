#pragma once

#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

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

    bool canAdvanceToNext() const;
    bool canGoToPrev() const;
    QString getNextModule() const;
    QString getPrevModule() const;

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
