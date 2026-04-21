#pragma once

#include <QString>

namespace InterModuleTest {

inline QString senderModuleId()
{
    return QStringLiteral("intermoduletest_a");
}

inline QString receiverModuleId()
{
    return QStringLiteral("intermoduletest_b");
}

inline QString sendTextCommand()
{
    return QStringLiteral("send_text_to_receiver");
}

inline QString displayTextMethod()
{
    return QStringLiteral("display_text");
}

inline QString receiverTextUpdatedEvent()
{
    return QStringLiteral("intermodule_text_updated");
}

inline QString previewTextEvent()
{
    return QStringLiteral("intermodule_preview_text");
}

} // namespace InterModuleTest