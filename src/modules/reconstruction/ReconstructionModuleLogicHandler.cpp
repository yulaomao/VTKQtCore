#include "ReconstructionModuleLogicHandler.h"

#include "ReconstructionUiCommands.h"

ReconstructionModuleLogicHandler::ReconstructionModuleLogicHandler(QObject* parent)
    : ModuleLogicHandler(QStringLiteral("reconstruction"), parent)
{
}

void ReconstructionModuleLogicHandler::handleAction(const UiAction& action)
{
    if (action.actionType != UiAction::CustomAction) {
        return;
    }

    const QString command = action.payload.value(QStringLiteral("command")).toString().trimmed();

    if (command == ReconstructionUiCommands::startReconstruction()) {
        m_isReconstructionDone = false;
        emitStatus(QStringLiteral("正在执行重建..."));

        LogicNotification notification = LogicNotification::create(
            LogicNotification::StageChanged,
            LogicNotification::CurrentModule,
            {{QStringLiteral("status"), QStringLiteral("Reconstructing")},
             {QStringLiteral("reconstructionDone"), false}});
        notification.setSourceActionId(action.actionId);
        emit logicNotification(notification);
        return;
    }

    if (command == ReconstructionUiCommands::resetReconstruction()) {
        m_isReconstructionDone = false;
        emitStatus(QStringLiteral("重建已重置。"), false);

        LogicNotification notification = LogicNotification::create(
            LogicNotification::StageChanged,
            LogicNotification::CurrentModule,
            {{QStringLiteral("status"), QStringLiteral("Idle")},
             {QStringLiteral("reconstructionDone"), false}});
        notification.setSourceActionId(action.actionId);
        emit logicNotification(notification);
        return;
    }
}

void ReconstructionModuleLogicHandler::handleStateSample(const StateSample& sample)
{
    Q_UNUSED(sample);
}

void ReconstructionModuleLogicHandler::onModuleActivated()
{
    emitStatus(m_statusText, m_isReconstructionDone);
}

void ReconstructionModuleLogicHandler::onModuleDeactivated()
{
}

void ReconstructionModuleLogicHandler::onResync()
{
    emitStatus(QStringLiteral("重建模块已重同步。"), m_isReconstructionDone);
}

void ReconstructionModuleLogicHandler::emitStatus(const QString& status, bool reconstructionDone)
{
    m_statusText = status;
    m_isReconstructionDone = reconstructionDone;

    emit logicNotification(LogicNotification::create(
        LogicNotification::StageChanged,
        LogicNotification::CurrentModule,
        {{QStringLiteral("status"), m_statusText},
         {QStringLiteral("reconstructionDone"), m_isReconstructionDone}}));
}
