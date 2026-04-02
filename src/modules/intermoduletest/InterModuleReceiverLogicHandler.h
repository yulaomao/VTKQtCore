#pragma once

#include "logic/registry/ModuleLogicHandler.h"

class InterModuleReceiverLogicHandler : public ModuleLogicHandler
{
    Q_OBJECT

public:
    explicit InterModuleReceiverLogicHandler(QObject* parent = nullptr);

    void handleAction(const UiAction& action) override;
    ModuleInvokeResult handleModuleInvoke(const ModuleInvokeRequest& request) override;

private:
    ModuleInvokeResult displayText(const QVariantMap& payload,
                                   const QString& sourceModule,
                                   const QString& sourceActionId);
    void emitTextUpdatedNotification(const QString& sourceModule,
                                     const QString& sourceActionId);

    QString m_lastText = QStringLiteral("等待模块 A 发送文本");
};