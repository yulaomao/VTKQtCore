#pragma once

#include <utility>

#include "ModuleUiEvent.h"

namespace ModuleUiEventBinding {

template <typename Sender, typename Receiver, typename Handler>
QMetaObject::Connection bind(Sender* notificationSource,
                             void (Sender::*notificationSignal)(const LogicNotification&),
                             const QString& moduleId,
                             const QString& eventName,
                             Receiver* receiver,
                             Handler&& handler)
{
    if (!notificationSource || !receiver) {
        return QMetaObject::Connection();
    }

    const QString normalizedModuleId = moduleId.trimmed();
    const QString normalizedEventName = eventName.trimmed();

    return QObject::connect(
        notificationSource,
        notificationSignal,
        receiver,
        [normalizedModuleId,
         normalizedEventName,
         fn = std::forward<Handler>(handler)](const LogicNotification& notification) mutable {
            if (!ModuleUiEvent::isNotification(notification,
                                               normalizedModuleId,
                                               normalizedEventName)) {
                return;
            }

            fn(ModuleUiEvent::extractCustomPayload(notification.payload));
        });
}

} // namespace ModuleUiEventBinding