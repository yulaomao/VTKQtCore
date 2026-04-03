#include "kneePlaneLogicHandler.h"

#include "kneePlaneContants.h"

KneePlaneLogicHandler::KneePlaneLogicHandler(QObject* parent)
    : ModuleLogicHandler(QStringLiteral("kneePlane"), parent)
{
}   

void KneePlaneLogicHandler::handleAction(const UiAction& action)
{
    if (action.actionType != UiAction::CustomAction) {
        return;
    }

    const QString command = action.payload.value(QStringLiteral("command")).toString().trimmed();
    if (command.isEmpty()) {
        return;
    }

    // Handle commands here, for example:
    // if (command == KneePlaneConstants::someCommand()) {
    //     // Process the command
    //     return;
    // }

    emitShellError(
        QStringLiteral("UNKNOWN_COMMAND"),
        QStringLiteral("Received unknown command: '%1'").arg(command),
        action.actionId);
} 


void KneePlaneLogicHandler::emitShellError(const QString& errorCode,
                                           const QString& message,
                                           const QString& sourceActionId)
{
    LogicNotification notification = LogicNotification::create(
        LogicNotification::ErrorOccurred,
        LogicNotification::Shell,
        {{QStringLiteral("errorCode"), errorCode},
         {QStringLiteral("message"), message},
         {QStringLiteral("recoverable"), true},
         {QStringLiteral("suggestedAction"), QStringLiteral("检查命令格式与模块状态。")}});
    notification.setSourceActionId(sourceActionId);
    notification.setLevel(LogicNotification::Warning);
    emit logicNotification(notification);
}



void KneePlaneLogicHandler::initAllTransNode(SceneGraph* scene)
{
    if (!scene) {
        return;
    }

    // Initialize all necessary transform nodes in the scene graph here
    QString name = "FemurJTTransNode";
    TransformNode* node = scene->getNodeById();


}

void KneePlaneLogicHandler::initAllPointsNode(SceneGraph* scene)
{
    if (!scene) {
        return;
    }

    // Initialize all necessary points nodes in the scene graph here
    // For example:
    // ensurePointsNode(scene, QStringLiteral("kneePlanePoints"));
}

void KneePlaneLogicHandler::initAllModelNode(SceneGraph* scene)
{
    if (!scene) {
        return;
    }

    // Initialize all necessary model nodes in the scene graph here
    // For example:
    // ensureModelNode(scene, QStringLiteral("kneePlaneModel"));
}

void KneePlaneLogicHandler::loadModelFromRedis(){
    // Load model data from Redis and update the scene graph accordingly
    // For example:
    // QVariantMap modelData = readRedisJsonValue(kneePlaneModelRedisKey());
    // if (!modelData.isEmpty()) {
    //     updateModelNodeFromData(modelData);
    // }
}

void KneePlaneLogicHandler::initAllNodesByRedis(SceneGraph* scene)
{
    if (!scene) {
        return;
    }

    initAllTransNode(scene);
    initAllPointsNode(scene);
    initAllModelNode(scene);
    loadModelFromRedis();
}