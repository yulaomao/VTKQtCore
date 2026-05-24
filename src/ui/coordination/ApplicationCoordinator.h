#pragma once

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariantMap>

#include "contracts/UiAction.h"
#include "contracts/LogicNotification.h"

class ILogicRuntimePort;
class GlobalUiManager;
class ModuleCoordinator;
class UiActionDispatcher;

class ApplicationCoordinator : public QObject
{
    Q_OBJECT

public:
    ApplicationCoordinator(ILogicRuntimePort* runtimePort,
                           GlobalUiManager* globalUiMgr,
                           QObject* parent = nullptr);
    ~ApplicationCoordinator() override = default;

    void registerModuleCoordinator(ModuleCoordinator* coordinator);
    ModuleCoordinator* getModuleCoordinator(const QString& moduleId) const;
    UiActionDispatcher* getActionDispatcher() const;
    QString getCurrentModule() const;

public slots:
    void requestResync(const QString& reason = QStringLiteral("manual"));

    void onShellNotification(const LogicNotification& notification);

signals:
    void shellAction(const UiAction& action);
    void currentModuleChanged(const QString& moduleId);
    void connectionStateChanged(const QString& state);
    void healthSnapshotChanged(const QVariantMap& snapshot);

private:
    void updateCurrentModule(const QString& moduleId);

    GlobalUiManager* m_globalUiManager;
    UiActionDispatcher* m_actionDispatcher;
    QMap<QString, ModuleCoordinator*> m_moduleCoordinators;
    QString m_currentModuleId;
};
