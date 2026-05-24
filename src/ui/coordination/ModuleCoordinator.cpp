#include "ModuleCoordinator.h"
#include "logic/runtime/ILogicRuntimePort.h"
#include "UiActionDispatcher.h"

ModuleCoordinator::ModuleCoordinator(const QString& moduleId, ILogicRuntimePort* runtimePort,
                                     QObject* parent)
    : QObject(parent)
    , m_moduleId(moduleId)
    , m_actionDispatcher(new UiActionDispatcher(moduleId, runtimePort, this))
    , m_mainPage(nullptr)
{
    connect(m_actionDispatcher, &UiActionDispatcher::actionDispatched,
            this, [this](const UiAction& action) {
                emit moduleAction(action);
            });
}

QString ModuleCoordinator::getModuleId() const
{
    return m_moduleId;
}

void ModuleCoordinator::setMainPage(QWidget* page)
{
    m_mainPage = page;
}

QWidget* ModuleCoordinator::getMainPage() const
{
    return m_mainPage;
}

UiActionDispatcher* ModuleCoordinator::getActionDispatcher() const
{
    return m_actionDispatcher;
}

void ModuleCoordinator::addSupplementaryView(QWidget* widget)
{
    if (!widget) {
        return;
    }

    m_supplementaryViews.append(widget);
}

QVector<QWidget*> ModuleCoordinator::getSupplementaryViews() const
{
    return m_supplementaryViews;
}

void ModuleCoordinator::activate()
{
    if (m_mainPage) {
        m_mainPage->show();
    }
    for (auto* w : m_supplementaryViews) {
        w->show();
    }
    emit activated();
}

void ModuleCoordinator::deactivate()
{
    if (m_mainPage) {
        m_mainPage->hide();
    }
    for (auto* w : m_supplementaryViews) {
        w->hide();
    }
    emit deactivated();
}

void ModuleCoordinator::onModuleNotification(const LogicNotification& notification)
{
    if (notification.targetScope == LogicNotification::CurrentModule ||
        notification.targetScope == LogicNotification::AllModules ||
        (notification.targetScope == LogicNotification::ModuleList &&
         notification.targetModules.contains(m_moduleId))) {
        emit notificationForPage(notification);
    }
}
