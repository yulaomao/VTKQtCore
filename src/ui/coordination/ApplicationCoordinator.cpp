#include "ApplicationCoordinator.h"
#include "ModuleCoordinator.h"
#include "ui/globalui/GlobalUiManager.h"
#include "logic/runtime/ILogicRuntimePort.h"
#include "UiActionDispatcher.h"

ApplicationCoordinator::ApplicationCoordinator(ILogicRuntimePort* runtimePort,
                                               GlobalUiManager* globalUiMgr,
                                               QObject* parent)
    : QObject(parent)
    , m_globalUiManager(globalUiMgr)
    , m_actionDispatcher(new UiActionDispatcher(QStringLiteral("shell"), runtimePort, this))
{
    connect(m_actionDispatcher, &UiActionDispatcher::actionDispatched,
            this, [this](const UiAction& action) {
                emit shellAction(action);
            });
}

void ApplicationCoordinator::registerModuleCoordinator(ModuleCoordinator* coordinator)
{
    if (coordinator) {
        m_moduleCoordinators.insert(coordinator->getModuleId(), coordinator);
    }
}

ModuleCoordinator* ApplicationCoordinator::getModuleCoordinator(
    const QString& moduleId) const
{
    return m_moduleCoordinators.value(moduleId, nullptr);
}

UiActionDispatcher* ApplicationCoordinator::getActionDispatcher() const
{
    return m_actionDispatcher;
}

void ApplicationCoordinator::updateCurrentModule(const QString& moduleId)
{
    if (moduleId == m_currentModuleId) {
        return;
    }

    // Deactivate current module
    auto* oldCoord = m_moduleCoordinators.value(m_currentModuleId, nullptr);
    if (oldCoord) {
        oldCoord->deactivate();
    }

    m_currentModuleId = moduleId;

    auto* newCoord = m_moduleCoordinators.value(moduleId, nullptr);
    if (newCoord) {
        newCoord->activate();
    }

    emit currentModuleChanged(moduleId);
}

QString ApplicationCoordinator::getCurrentModule() const
{
    return m_currentModuleId;
}

void ApplicationCoordinator::requestResync(const QString& reason)
{
    if (m_actionDispatcher) {
        m_actionDispatcher->requestResync(reason);
    }
}

void ApplicationCoordinator::onShellNotification(const LogicNotification& notification)
{
    switch (notification.eventType) {
    case LogicNotification::ModuleChanged: {
        QString newModuleId = notification.payload.value("newModule").toString();
        if (!newModuleId.isEmpty()) {
            updateCurrentModule(newModuleId);
        }
        break;
    }

    case LogicNotification::ConnectionStateChanged:
        // Update status display via global UI
        if (m_globalUiManager) {
            QString state = notification.payload.value("state").toString();
            emit connectionStateChanged(state);
            if (state == "Disconnected") {
                m_globalUiManager->showNotification(
                    "Connection lost", "error");
            } else if (state == "Degraded") {
                m_globalUiManager->showNotification(
                    "Connection degraded", "warning");
            } else {
                m_globalUiManager->hideNotification();
            }
        }
        break;

    case LogicNotification::ActiveModuleChanged:
        if (notification.payload.contains(QStringLiteral("currentModule"))) {
            const QString moduleId = notification.payload.value(
                QStringLiteral("currentModule")).toString();
            if (!moduleId.isEmpty() && moduleId != m_currentModuleId) {
                updateCurrentModule(moduleId);
            }
        }
        break;

    case LogicNotification::ErrorOccurred:
        if (m_globalUiManager) {
            QString errorCode = notification.payload.value("errorCode").toString();
            QString message = notification.payload.value("message").toString();
            bool recoverable = notification.payload.value("recoverable", true).toBool();
            QString suggestedAction =
                notification.payload.value("suggestedAction").toString();
            if (notification.level == LogicNotification::Warning) {
                m_globalUiManager->showNotification(message, QStringLiteral("warning"));
            } else {
                m_globalUiManager->showError(errorCode, message,
                                             recoverable, suggestedAction);
            }
        }
        break;

    case LogicNotification::CustomEvent:
        if (notification.payload.value(QStringLiteral("eventName")).toString() ==
            QStringLiteral("communication_health")) {
            emit healthSnapshotChanged(notification.payload);
        }
        break;

    default:
        break;
    }

    // Forward module-scoped notifications to appropriate coordinators
    if (notification.targetScope == LogicNotification::CurrentModule) {
        if (auto* coord = m_moduleCoordinators.value(m_currentModuleId, nullptr)) {
            coord->onModuleNotification(notification);
        }
    } else if (notification.targetScope == LogicNotification::AllModules) {
        for (auto* coord : m_moduleCoordinators) {
            coord->onModuleNotification(notification);
        }
    } else if (notification.targetScope == LogicNotification::ModuleList) {
        for (const auto& modId : notification.targetModules) {
            if (auto* coord = m_moduleCoordinators.value(modId, nullptr)) {
                coord->onModuleNotification(notification);
            }
        }
    }
}

