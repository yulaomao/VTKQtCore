#pragma once

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "contracts/UiAction.h"
#include "contracts/LogicNotification.h"

class ILogicGateway;
class PageManager;
class GlobalUiManager;
class ModuleCoordinator;
class WorkspaceShell;

class ApplicationCoordinator : public QObject
{
    Q_OBJECT

public:
    ApplicationCoordinator(ILogicGateway* gateway, PageManager* pageMgr,
                           GlobalUiManager* globalUiMgr,
                           WorkspaceShell* workspaceShell,
                           QObject* parent = nullptr);
    ~ApplicationCoordinator() override = default;

    void registerModuleCoordinator(ModuleCoordinator* coordinator);
    ModuleCoordinator* getModuleCoordinator(const QString& moduleId) const;
    void setCurrentModule(const QString& moduleId);
    QString getCurrentModule() const;

public slots:
    void requestSwitchModule(const QString& moduleId);
    void requestResync(const QString& reason = QStringLiteral("manual"));

    void onShellNotification(const LogicNotification& notification);

signals:
    void shellAction(const UiAction& action);
    void currentModuleChanged(const QString& moduleId);
    void connectionStateChanged(const QString& state);
    void healthSnapshotChanged(const QVariantMap& snapshot);

private:
    void dispatchShellAction(UiAction::ActionType type,
                             const QVariantMap& payload = {});

    ILogicGateway* m_gateway;
    PageManager* m_pageManager;
    GlobalUiManager* m_globalUiManager;
    WorkspaceShell* m_workspaceShell;
    QMap<QString, ModuleCoordinator*> m_moduleCoordinators;
    QString m_currentModuleId;
};
