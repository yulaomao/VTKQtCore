#pragma once

#include <QSet>
#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QWidget>

class QLabel;
class QPushButton;

class ShellStatusBar : public QWidget
{
    Q_OBJECT

public:
    explicit ShellStatusBar(QWidget* parent = nullptr);

    void setWorkflowSequence(const QStringList& modules);

public slots:
    void setCurrentModule(const QString& moduleId);
    void setEnterableModules(const QStringList& moduleIds);
    void setConnectionState(const QString& state);
    void setHealthSnapshot(const QVariantMap& snapshot);
    void setWorkflowDecision(const QString& reasonCode, const QString& message);

signals:
    void prevRequested();
    void nextRequested();
    void resyncRequested(const QString& reason);

private:
    void refreshState();

    QStringList m_modules;
    QString m_currentModule;
    QSet<QString> m_enterableModules;
    QString m_connectionState = QStringLiteral("Disconnected");
    QString m_healthState = QStringLiteral("offline");
    QString m_workflowReasonCode;
    QString m_workflowReasonMessage;
    QLabel* m_stepLabel = nullptr;
    QLabel* m_connectionLabel = nullptr;
    QLabel* m_healthLabel = nullptr;
    QPushButton* m_prevButton = nullptr;
    QPushButton* m_nextButton = nullptr;
};