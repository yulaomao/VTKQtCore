#pragma once

#include "logic/registry/ModuleLogicHandler.h"


#include "logic/scene/SceneGraph.h"
#include "logic/scene/nodes/PointNode.h"
#include "logic/scene/nodes/TransformNode.h"

class KneePlaneLogicHandler : public ModuleLogicHandler
{
    Q_OBJECT

public:
    explicit KneePlaneLogicHandler(QObject* parent = nullptr);

    void handleAction(const UiAction& action) override;

    void initAllNodesByRedis(SceneGraph* scene);

private:
    void emitShellError(const QString& errorCode,
                        const QString& message,
                        const QString& sourceActionId);


    void initAllTransNode(SceneGraph* scene);
    void initAllPointsNode(SceneGraph* scene);
    void initAllModelNode(SceneGraph* scene);
    void loadModelFromRedis();
    
};