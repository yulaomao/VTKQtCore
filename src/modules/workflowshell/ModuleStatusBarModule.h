#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QWidget>

class QLabel;
class QPushButton;

class ModuleStatusBarModule : public QWidget
{
    Q_OBJECT

public:
    explicit ModuleStatusBarModule(QWidget* parent = nullptr);

    void setModuleDisplayOrder(const QStringList& modules);

public slots:
    void setCurrentModule(const QString& moduleId);
    void setConnectionState(const QString& state);
    void setHealthSnapshot(const QVariantMap& snapshot);

signals:
    void resyncRequested(const QString& reason);

private:
    void refreshState();
    static QString formatModuleLabel(const QString& moduleId);

    QStringList m_modules;
    QString m_currentModule;
    QString m_connectionState = QStringLiteral("Disconnected");
    QString m_healthState = QStringLiteral("offline");
    QLabel* m_stepLabel = nullptr;
    QLabel* m_connectionLabel = nullptr;
    QLabel* m_healthLabel = nullptr;
};
