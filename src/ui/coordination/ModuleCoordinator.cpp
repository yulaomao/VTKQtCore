#include "ModuleCoordinator.h"
#include "logic/gateway/ILogicGateway.h"
#include "UiActionDispatcher.h"

ModuleCoordinator::ModuleCoordinator(const QString& moduleId, ILogicGateway* gateway,
                                     QObject* parent)
    : QObject(parent)
    , m_moduleId(moduleId)
    , m_gateway(gateway)
    , m_actionDispatcher(new UiActionDispatcher(moduleId, gateway, this))
    , m_mainPage(nullptr)
{
    connect(m_actionDispatcher, &UiActionDispatcher::actionDispatched,
            this, [this](const UiAction& action, bool) {
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

void ModuleCoordinator::addAuxiliaryWidget(QWidget* widget,
                                           AuxiliaryRegion region)
{
    if (!widget) {
        return;
    }

    if (region == AuxiliaryRegion::Bottom) {
        m_bottomAuxiliaryWidgets.append(widget);
    } else {
        m_rightAuxiliaryWidgets.append(widget);
    }
}

QVector<QWidget*> ModuleCoordinator::getAuxiliaryWidgets(AuxiliaryRegion region) const
{
    return region == AuxiliaryRegion::Bottom
        ? m_bottomAuxiliaryWidgets
        : m_rightAuxiliaryWidgets;
}

void ModuleCoordinator::activate()
{
    if (m_mainPage) {
        m_mainPage->show();
    }
    for (auto* w : m_rightAuxiliaryWidgets) {
        w->show();
    }
    for (auto* w : m_bottomAuxiliaryWidgets) {
        w->show();
    }
    emit activated();
}

void ModuleCoordinator::deactivate()
{
    if (m_mainPage) {
        m_mainPage->hide();
    }
    for (auto* w : m_rightAuxiliaryWidgets) {
        w->hide();
    }
    for (auto* w : m_bottomAuxiliaryWidgets) {
        w->hide();
    }
    emit deactivated();
}

void ModuleCoordinator::sendModuleAction(UiAction::ActionType type,
                                         const QVariantMap& payload)
{
    if (m_actionDispatcher) {
        m_actionDispatcher->sendAction(type, payload);
    }
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
