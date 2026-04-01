#include "NavigationModuleLogicHandler.h"

#include "logic/scene/SceneGraph.h"
#include "logic/scene/nodes/TransformNode.h"

namespace {

QVariantMap normalizedSampleData(const StateSample& sample)
{
    QVariantMap payload = sample.data.value(QStringLiteral("value")).toMap();
    return payload.isEmpty() ? sample.data : payload;
}

bool extractPosition(const QVariantMap& payload, double& x, double& y, double& z)
{
    if (payload.contains(QStringLiteral("position"))) {
        const QVariantList values = payload.value(QStringLiteral("position")).toList();
        if (values.size() >= 3) {
            x = values.at(0).toDouble();
            y = values.at(1).toDouble();
            z = values.at(2).toDouble();
            return true;
        }
    }

    if (payload.contains(QStringLiteral("x")) &&
        payload.contains(QStringLiteral("y")) &&
        payload.contains(QStringLiteral("z"))) {
        x = payload.value(QStringLiteral("x")).toDouble();
        y = payload.value(QStringLiteral("y")).toDouble();
        z = payload.value(QStringLiteral("z")).toDouble();
        return true;
    }

    return false;
}

bool extractMatrix(const QVariantMap& payload, double matrix[16])
{
    const QVariantList values = payload.value(QStringLiteral("matrix")).toList();
    if (values.size() < 16) {
        return false;
    }

    for (int index = 0; index < 16; ++index) {
        matrix[index] = values.at(index).toDouble();
    }
    return true;
}

void setTranslationMatrix(double matrix[16], double x, double y, double z)
{
    for (int index = 0; index < 16; ++index) {
        matrix[index] = 0.0;
    }

    matrix[0] = 1.0;
    matrix[5] = 1.0;
    matrix[10] = 1.0;
    matrix[15] = 1.0;
    matrix[12] = x;
    matrix[13] = y;
    matrix[14] = z;
}

} // namespace

NavigationModuleLogicHandler::NavigationModuleLogicHandler(QObject* parent)
    : ModuleLogicHandler(QStringLiteral("navigation"), parent)
{
}

void NavigationModuleLogicHandler::onModuleActivated()
{
    if (!ensureToolTransformNode(getSceneGraph())) {
        return;
    }

    emitNavigationState();
}

void NavigationModuleLogicHandler::handleAction(const UiAction& action)
{
    if (action.actionType == UiAction::StartNavigation) {
        ensureToolTransformNode(getSceneGraph());
        m_navigating = true;
        m_navigationStatus = QStringLiteral("Navigating");
        emitNavigationState(action.actionId);
        return;
    }

    if (action.actionType == UiAction::StopNavigation) {
        ensureToolTransformNode(getSceneGraph());
        m_navigating = false;
        m_navigationStatus = QStringLiteral("Idle");
        emitNavigationState(action.actionId);
    }
}

void NavigationModuleLogicHandler::handleStateSample(const StateSample& sample)
{
    SceneGraph* scene = getSceneGraph();
    auto* transformNode = ensureToolTransformNode(scene);
    if (!transformNode) {
        return;
    }

    const QVariantMap payloadData = normalizedSampleData(sample);
    bool changed = false;

    if (payloadData.contains(QStringLiteral("navigating"))) {
        m_navigating = payloadData.value(QStringLiteral("navigating")).toBool();
        changed = true;
    }

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    const bool hasPosition = extractPosition(payloadData, x, y, z);

    if (payloadData.contains(QStringLiteral("matrix"))) {
        double matrix[16];
        if (extractMatrix(payloadData, matrix)) {
            transformNode->setMatrixTransformToParent(matrix);
            changed = true;
        }
    } else if (hasPosition) {
        double matrix[16];
        setTranslationMatrix(matrix, x, y, z);
        transformNode->setMatrixTransformToParent(matrix);
        changed = true;
    }

    const QString status = payloadData.value(QStringLiteral("status")).toString();
    if (!status.isEmpty()) {
        m_navigationStatus = status;
        changed = true;
    } else if (changed || m_navigationStatus.isEmpty()) {
        m_navigationStatus = m_navigating
            ? QStringLiteral("Navigating")
            : QStringLiteral("Idle");
    }

    if (changed || hasPosition) {
        emitNavigationState(QString(), sample.sampleId);
    }
}

void NavigationModuleLogicHandler::onModuleDeactivated()
{
}

void NavigationModuleLogicHandler::onResync()
{
    emitNavigationState();
}

TransformNode* NavigationModuleLogicHandler::ensureToolTransformNode(SceneGraph* scene)
{
    if (!scene) {
        return nullptr;
    }

    if (!m_toolTransformNodeId.isEmpty()) {
        if (auto* existing = scene->getNodeById<TransformNode>(m_toolTransformNodeId)) {
            return existing;
        }
        m_toolTransformNodeId.clear();
    }

    if (auto* existing = findToolTransformNode(scene)) {
        m_toolTransformNodeId = existing->getNodeId();
        return existing;
    }

    auto* transformNode = new TransformNode(scene);
    transformNode->setTransformKind(QStringLiteral("tool_tracking"));
    transformNode->setShowAxes(true);
    DisplayTarget navigationTarget;
    navigationTarget.visible = true;
    navigationTarget.layer = 3;
    transformNode->setWindowDisplayTarget(QStringLiteral("navigation_main"), navigationTarget);
    scene->addNode(transformNode);
    m_toolTransformNodeId = transformNode->getNodeId();
    return transformNode;
}

TransformNode* NavigationModuleLogicHandler::findToolTransformNode(SceneGraph* scene) const
{
    if (!scene) {
        return nullptr;
    }

    const QVector<TransformNode*> transformNodes = scene->getAllTransformNodes();
    for (TransformNode* transformNode : transformNodes) {
        if (transformNode && transformNode->getTransformKind() == QStringLiteral("tool_tracking")) {
            return transformNode;
        }
    }

    return nullptr;
}

void NavigationModuleLogicHandler::emitNavigationState(const QString& sourceActionId,
                                                       const QString& sourceSampleId) const
{
    QVariantMap payload;
    payload.insert(QStringLiteral("toolTransformNodeId"), m_toolTransformNodeId);
    payload.insert(QStringLiteral("navigating"), m_navigating);
    payload.insert(QStringLiteral("status"), m_navigationStatus);

    SceneGraph* scene = getSceneGraph();
    if (scene) {
        if (auto* transformNode = scene->getNodeById<TransformNode>(m_toolTransformNodeId)) {
            double matrix[16];
            transformNode->getMatrixTransformToParent(matrix);
            payload.insert(QStringLiteral("x"), matrix[12]);
            payload.insert(QStringLiteral("y"), matrix[13]);
            payload.insert(QStringLiteral("z"), matrix[14]);
        }
    }

    if (!sourceSampleId.isEmpty()) {
        payload.insert(QStringLiteral("sourceSampleId"), sourceSampleId);
    }

    LogicNotification notification = LogicNotification::create(
        LogicNotification::StageChanged,
        LogicNotification::CurrentModule,
        payload);
    notification.setSourceActionId(sourceActionId);
    emit logicNotification(notification);
}
