#pragma once

#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QWidget>

class QVBoxLayout;
class QPushButton;

class ShellWorkflowMenu : public QWidget
{
    Q_OBJECT

public:
    explicit ShellWorkflowMenu(QWidget* parent = nullptr);

    void setWorkflowSequence(const QStringList& modules);

public slots:
    void setCurrentModule(const QString& moduleId);
    void setEnterableModules(const QStringList& moduleIds);

signals:
    void moduleSelected(const QString& moduleId);

private:
    void rebuildButtons();
    void refreshButtonState();

    QStringList m_modules;
    QString m_currentModule;
    QSet<QString> m_enterableModules;
    QVBoxLayout* m_buttonLayout = nullptr;
    QMap<QString, QPushButton*> m_buttons;
};