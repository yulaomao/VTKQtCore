#pragma once

#include <QObject>
#include <QMap>
#include <QString>

#include "contracts/UiAction.h"
#include "contracts/LogicNotification.h"

class ILogicGateway;
class PageManager;
class GlobalUiManager;
class ModuleCoordinator;

class ApplicationCoordinator : public QObject
{
    Q_OBJECT

public:
    ApplicationCoordinator(ILogicGateway* gateway, PageManager* pageMgr,
                           GlobalUiManager* globalUiMgr,
                           QObject* parent = nullptr);
    ~ApplicationCoordinator() override = default;

    void registerModuleCoordinator(ModuleCoordinator* coordinator);
    ModuleCoordinator* getModuleCoordinator(const QString& moduleId) const;
    void setCurrentModule(const QString& moduleId);
    QString getCurrentModule() const;

public slots:
    void onShellNotification(const LogicNotification& notification);

signals:
    void shellAction(const UiAction& action);

private:
    ILogicGateway* m_gateway;
    PageManager* m_pageManager;
    GlobalUiManager* m_globalUiManager;
    QMap<QString, ModuleCoordinator*> m_moduleCoordinators;
    QString m_currentModuleId;
};
