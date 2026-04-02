#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QWidget>

class QLabel;
class QPushButton;
class UiActionDispatcher;

class ModuleStatusBarModule : public QWidget
{
    Q_OBJECT

public:
    explicit ModuleStatusBarModule(QWidget* parent = nullptr);

    void setActionDispatcher(UiActionDispatcher* dispatcher);
    void setModuleDisplayOrder(const QStringList& modules);

public slots:
    void setCurrentModule(const QString& moduleId);
    void setConnectionState(const QString& state);
    void setHealthSnapshot(const QVariantMap& snapshot);

private:
    void refreshState();
    static QString formatModuleLabel(const QString& moduleId);

    QStringList m_modules;
    QString m_currentModule;
    QString m_connectionState = QStringLiteral("Disconnected");
    QString m_healthState = QStringLiteral("offline");
    UiActionDispatcher* m_actionDispatcher = nullptr;
    QLabel* m_stepLabel = nullptr;
    QLabel* m_connectionLabel = nullptr;
    QLabel* m_healthLabel = nullptr;
};
