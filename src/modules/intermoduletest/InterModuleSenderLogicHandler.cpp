#include "InterModuleSenderLogicHandler.h"

#include "InterModuleTestConstants.h"

InterModuleSenderLogicHandler::InterModuleSenderLogicHandler(QObject* parent)
    : ModuleLogicHandler(InterModuleTest::senderModuleId(), parent)
{
}

void InterModuleSenderLogicHandler::handleAction(const UiAction& action)
{
    if (action.actionType != UiAction::CustomAction) {
        return;
    }

    const QString command = action.payload.value(QStringLiteral("command")).toString().trimmed();
    if (command != InterModuleTest::sendTextCommand()) {
        return;
    }

    const QString text = action.payload.value(QStringLiteral("text")).toString().trimmed();
    if (text.isEmpty()) {
        emitShellError(
            QStringLiteral("INTERMODULE_TEST_TEXT_EMPTY"),
            QStringLiteral("模块 A 发送内容为空。"),
            action.actionId);
        return;
    }

    const ModuleInvokeResult result = invokeModule(
        InterModuleTest::receiverModuleId(),
        InterModuleTest::displayTextMethod(),
        {{QStringLiteral("text"), text},
         {QStringLiteral("sourceActionId"), action.actionId}});
    if (!result.ok) {
        emitShellError(
            result.errorCode.isEmpty()
                ? QStringLiteral("INTERMODULE_TEST_SEND_FAILED")
                : result.errorCode,
            result.message.isEmpty()
                ? QStringLiteral("模块 B 未能接收来自模块 A 的文本。")
                : result.message,
            action.actionId);
    }
}

void InterModuleSenderLogicHandler::emitShellError(const QString& errorCode,
                                                   const QString& message,
                                                   const QString& sourceActionId)
{
    LogicNotification notification = LogicNotification::create(
        LogicNotification::ErrorOccurred,
        LogicNotification::Shell,
        {{QStringLiteral("errorCode"), errorCode},
         {QStringLiteral("message"), message},
         {QStringLiteral("recoverable"), true},
         {QStringLiteral("suggestedAction"), QStringLiteral("检查模块 A 输入内容与模块 B 注册状态。")}});
    notification.setSourceActionId(sourceActionId);
    notification.setLevel(LogicNotification::Warning);
    emit logicNotification(notification);
}