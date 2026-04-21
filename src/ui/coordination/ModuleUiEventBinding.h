#pragma once

#include <utility>

#include "ModuleUiEvent.h"
#include "logic/gateway/ILogicGateway.h"

namespace ModuleUiEventBinding {

template <typename Receiver, typename Handler>
QMetaObject::Connection bind(ILogicGateway* gateway,
                             const QString& moduleId,
                             const QString& eventName,
                             Receiver* receiver,
                             Handler&& handler)
{
    if (!gateway || !receiver) {
        return QMetaObject::Connection();
    }

    const QString normalizedModuleId = moduleId.trimmed();
    const QString normalizedEventName = eventName.trimmed();

    return QObject::connect(
        gateway,
        &ILogicGateway::notificationReceived,
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