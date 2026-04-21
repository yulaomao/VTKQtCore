#pragma once

#include <QString>
#include <QVariantMap>

#include "contracts/LogicNotification.h"
#include "contracts/UiAction.h"

namespace ModuleUiEvent {

inline QString command()
{
    return QStringLiteral("dispatch_module_ui_event");
}

inline QString category()
{
    return QStringLiteral("module_ui_event");
}

inline QString commandKey()
{
    return QStringLiteral("command");
}

inline QString categoryKey()
{
    return QStringLiteral("eventCategory");
}

inline QString eventNameKey()
{
    return QStringLiteral("eventName");
}

inline QString sourceModuleKey()
{
    return QStringLiteral("sourceModule");
}

inline QVariantMap createActionPayload(const QString& eventName,
                                      const QVariantMap& payload = {})
{
    const QString normalizedEventName = eventName.trimmed();
    QVariantMap result = payload;
    result.insert(commandKey(), command());
    result.insert(eventNameKey(), normalizedEventName);
    return result;
}

inline bool isAction(const UiAction& action,
                     QString* eventName = nullptr)
{
    if (action.actionType != UiAction::CustomAction) {
        return false;
    }

    if (action.payload.value(commandKey()).toString().trimmed() != command()) {
        return false;
    }

    const QString normalizedEventName = action.payload.value(eventNameKey()).toString().trimmed();
    if (normalizedEventName.isEmpty()) {
        return false;
    }

    if (eventName) {
        *eventName = normalizedEventName;
    }
    return true;
}

inline QVariantMap extractCustomPayload(const QVariantMap& payload)
{
    QVariantMap result = payload;
    result.remove(commandKey());
    result.remove(QStringLiteral("targetModule"));
    result.remove(categoryKey());
    result.remove(sourceModuleKey());
    result.remove(eventNameKey());
    return result;
}

inline QVariantMap createNotificationPayload(const QString& eventName,
                                            const QString& sourceModule,
                                            const QVariantMap& payload = {})
{
    const QString normalizedEventName = eventName.trimmed();
    QVariantMap result = extractCustomPayload(payload);
    result.insert(categoryKey(), category());
    result.insert(eventNameKey(), normalizedEventName);

    const QString normalizedSourceModule = sourceModule.trimmed();
    if (!normalizedSourceModule.isEmpty()) {
        result.insert(sourceModuleKey(), normalizedSourceModule);
    }

    return result;
}

inline bool isNotification(const LogicNotification& notification,
                          const QString& moduleId,
                          const QString& eventName = QString())
{
    if (notification.eventType != LogicNotification::CustomEvent) {
        return false;
    }

    const QString normalizedModuleId = moduleId.trimmed();
    if (notification.targetScope != LogicNotification::AllModules) {
        if (notification.targetScope != LogicNotification::ModuleList) {
            return false;
        }

        if (!normalizedModuleId.isEmpty() &&
            !notification.targetModules.contains(normalizedModuleId)) {
            return false;
        }
    }

    if (notification.payload.value(categoryKey()).toString().trimmed() != category()) {
        return false;
    }

    const QString normalizedEventName = notification.payload.value(eventNameKey()).toString().trimmed();
    if (normalizedEventName.isEmpty()) {
        return false;
    }

    const QString expectedEventName = eventName.trimmed();
    return expectedEventName.isEmpty() || normalizedEventName == expectedEventName;
}

} // namespace ModuleUiEvent