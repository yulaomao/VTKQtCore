#include "ModuleLogicHandler.h"

ModuleLogicHandler::ModuleLogicHandler(const QString& moduleId, QObject* parent)
    : QObject(parent)
    , m_moduleId(moduleId)
{
}

QString ModuleLogicHandler::getModuleId() const
{
    return m_moduleId;
}

void ModuleLogicHandler::setSceneGraph(SceneGraph* scene)
{
    m_sceneGraph = scene;
}

SceneGraph* ModuleLogicHandler::getSceneGraph() const
{
    return m_sceneGraph;
}
