#include "ModuleCoordinator.h"
#include "logic/gateway/ILogicGateway.h"

ModuleCoordinator::ModuleCoordinator(const QString& moduleId, ILogicGateway* gateway,
                                     QObject* parent)
    : QObject(parent)
    , m_moduleId(moduleId)
    , m_gateway(gateway)
    , m_mainPage(nullptr)
{
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
    UiAction action = UiAction::create(type, m_moduleId, payload);
    emit moduleAction(action);
    if (m_gateway) {
        m_gateway->sendAction(action);
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
