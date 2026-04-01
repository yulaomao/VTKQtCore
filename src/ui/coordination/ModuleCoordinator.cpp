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

void ModuleCoordinator::addAuxiliaryWidget(QWidget* widget)
{
    if (widget) {
        m_auxiliaryWidgets.append(widget);
    }
}

QVector<QWidget*> ModuleCoordinator::getAuxiliaryWidgets() const
{
    return m_auxiliaryWidgets;
}

void ModuleCoordinator::activate()
{
    if (m_mainPage) {
        m_mainPage->show();
    }
    for (auto* w : m_auxiliaryWidgets) {
        w->show();
    }
}

void ModuleCoordinator::deactivate()
{
    if (m_mainPage) {
        m_mainPage->hide();
    }
    for (auto* w : m_auxiliaryWidgets) {
        w->hide();
    }
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
        notification.targetScope == LogicNotification::AllModules) {
        emit notificationForPage(notification);
    }
}
