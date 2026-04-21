#include "InterModuleReceiverLogicHandler.h"

#include "InterModuleTestConstants.h"

InterModuleReceiverLogicHandler::InterModuleReceiverLogicHandler(QObject* parent)
    : ModuleLogicHandler(InterModuleTest::receiverModuleId(), parent)
{
}

void InterModuleReceiverLogicHandler::handleAction(const UiAction& action)
{
    if (forwardModuleUiEventAction(action)) {
        return;
    }

    if (action.actionType != UiAction::CustomAction) {
        return;
    }

    const QString command = action.payload.value(QStringLiteral("command")).toString().trimmed();
    if (command != InterModuleTest::displayTextMethod()) {
        return;
    }

    displayText(action.payload, action.module, action.actionId);
}

ModuleInvokeResult InterModuleReceiverLogicHandler::handleModuleInvoke(
    const ModuleInvokeRequest& request)
{
    QString method = request.method.trimmed();
    if (method.isEmpty()) {
        method = request.payload.value(QStringLiteral("command")).toString().trimmed();
    }

    if (method != InterModuleTest::displayTextMethod()) {
        return ModuleInvokeResult::failure(
            QStringLiteral("INTERMODULE_TEST_UNSUPPORTED_METHOD"),
            QStringLiteral("模块 B 不支持方法 '%1'。" ).arg(method),
            {{QStringLiteral("sourceModule"), request.sourceModule}});
    }

    return displayText(
        request.payload,
        request.sourceModule,
        request.payload.value(QStringLiteral("sourceActionId")).toString());
}

ModuleInvokeResult InterModuleReceiverLogicHandler::displayText(
    const QVariantMap& payload,
    const QString& sourceModule,
    const QString& sourceActionId)
{
    const QString text = payload.value(QStringLiteral("text")).toString().trimmed();
    if (text.isEmpty()) {
        return ModuleInvokeResult::failure(
            QStringLiteral("INTERMODULE_TEST_RECEIVER_TEXT_EMPTY"),
            QStringLiteral("模块 B 收到的文本为空。"),
            {{QStringLiteral("sourceModule"), sourceModule}});
    }

    m_lastText = text;
    emitTextUpdatedNotification(sourceModule, sourceActionId);
    return ModuleInvokeResult::success(
        {{QStringLiteral("text"), m_lastText}},
        QStringLiteral("模块 B 已更新显示文本。"));
}

void InterModuleReceiverLogicHandler::emitTextUpdatedNotification(
    const QString& sourceModule,
    const QString& sourceActionId)
{
    QVariantMap payload;
    payload.insert(QStringLiteral("eventName"), InterModuleTest::receiverTextUpdatedEvent());
    payload.insert(QStringLiteral("text"), m_lastText);
    payload.insert(QStringLiteral("sourceModule"), sourceModule);

    LogicNotification notification = LogicNotification::create(
        LogicNotification::CustomEvent,
        LogicNotification::ModuleList,
        payload);
    notification.targetModules = QStringList{InterModuleTest::receiverModuleId()};
    notification.setSourceActionId(sourceActionId);
    emit logicNotification(notification);
}