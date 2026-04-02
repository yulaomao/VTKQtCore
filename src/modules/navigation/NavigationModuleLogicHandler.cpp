#include "NavigationModuleLogicHandler.h"

#include "logic/scene/SceneGraph.h"
#include "logic/scene/nodes/TransformNode.h"

#include <QDateTime>
#include <QVariantList>

namespace {

struct NavigationTransformDescriptor {
    const char* remoteId;
    const char* parentRemoteId;
    const char* displayName;
    const char* transformKind;
    double axesLength;
    int layer;
};

constexpr NavigationTransformDescriptor kNavigationTransformDescriptors[] = {
    {"navigation-world-transform", "", "World", "navigation_world", 44.0, 2},
    {"navigation-reference-transform", "navigation-world-transform", "Reference", "navigation_reference", 34.0, 2},
    {"navigation-patient-transform", "navigation-reference-transform", "Patient", "navigation_patient", 28.0, 2},
    {"navigation-instrument-transform", "navigation-reference-transform", "Instrument", "navigation_instrument", 22.0, 3},
    {"navigation-guide-transform", "navigation-instrument-transform", "Guide", "navigation_guide", 16.0, 3},
    {"navigation-tip-transform", "navigation-guide-transform", "Tip", "navigation_tip", 12.0, 3},
};

QString toQString(const char* text)
{
    return QString::fromLatin1(text);
}

const NavigationTransformDescriptor* findDescriptor(const QString& remoteId)
{
    for (const NavigationTransformDescriptor& descriptor : kNavigationTransformDescriptors) {
        if (remoteId == QLatin1String(descriptor.remoteId)) {
            return &descriptor;
        }
    }
    return nullptr;
}

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

bool extractMatrixToParent(const QVariantMap& payload, double matrix[16])
{
    const QVariantList rows = payload.value(QStringLiteral("matrixToParent")).toList();
    if (rows.size() != 4) {
        return false;
    }

    for (int rowIndex = 0; rowIndex < 4; ++rowIndex) {
        const QVariantList row = rows.at(rowIndex).toList();
        if (row.size() != 4) {
            return false;
        }
        for (int columnIndex = 0; columnIndex < 4; ++columnIndex) {
            matrix[columnIndex * 4 + rowIndex] = row.at(columnIndex).toDouble();
        }
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

void configureTrackedTransformNode(TransformNode* transformNode,
                                  const NavigationTransformDescriptor& descriptor)
{
    if (!transformNode) {
        return;
    }

    transformNode->setName(toQString(descriptor.displayName));
    transformNode->setAttribute(QStringLiteral("remoteTransformId"),
                                toQString(descriptor.remoteId));
    transformNode->setTransformKind(toQString(descriptor.transformKind));
    transformNode->setSourceSpace(toQString(descriptor.displayName).toLower());
    transformNode->setTargetSpace(QStringLiteral("navigation"));
    transformNode->setShowAxes(true);
    transformNode->setAxesLength(descriptor.axesLength);

    DisplayTarget target;
    target.visible = true;
    target.layer = descriptor.layer;
    transformNode->setWindowDisplayTarget(QStringLiteral("navigation_main"), target);
}

QVariantList buildTransformStatusList(const QMap<QString, qint64>& lastSampleTimestamps)
{
    QVariantList transforms;
    for (const NavigationTransformDescriptor& descriptor : kNavigationTransformDescriptors) {
        const QString remoteId = toQString(descriptor.remoteId);
        QVariantMap entry;
        entry.insert(QStringLiteral("nodeId"), remoteId);
        entry.insert(QStringLiteral("displayName"), toQString(descriptor.displayName));
        entry.insert(QStringLiteral("lastSampleTimestampMs"),
                     lastSampleTimestamps.value(remoteId, 0));
        transforms.append(entry);
    }
    return transforms;
}

} // namespace

NavigationModuleLogicHandler::NavigationModuleLogicHandler(QObject* parent)
    : ModuleLogicHandler(QStringLiteral("navigation"), parent)
{
}

void NavigationModuleLogicHandler::onModuleActivated()
{
    SceneGraph* scene = getSceneGraph();
    for (const NavigationTransformDescriptor& descriptor : kNavigationTransformDescriptors) {
        ensureTrackedTransformNode(scene, toQString(descriptor.remoteId));
    }

    emitNavigationState();
    emitTransformHealth(true);
}

void NavigationModuleLogicHandler::handleAction(const UiAction& action)
{
    if (action.actionType == UiAction::StartNavigation) {
        SceneGraph* scene = getSceneGraph();
        for (const NavigationTransformDescriptor& descriptor : kNavigationTransformDescriptors) {
            ensureTrackedTransformNode(scene, toQString(descriptor.remoteId));
        }
        m_navigating = true;
        m_navigationStatus = QStringLiteral("Navigating");
        emitNavigationState(action.actionId);
        emitTransformHealth(true);
        return;
    }

    if (action.actionType == UiAction::StopNavigation) {
        m_navigating = false;
        m_navigationStatus = QStringLiteral("Idle");
        emitNavigationState(action.actionId);
        emitTransformHealth(true);
        return;
    }

    if (action.actionType == UiAction::CustomAction) {
        Q_UNUSED(action);
    }
}

void NavigationModuleLogicHandler::handleStateSample(const StateSample& sample)
{
    const QVariantMap payloadData = normalizedSampleData(sample);

    if (payloadData.contains(QStringLiteral("nodeId")) &&
        payloadData.contains(QStringLiteral("matrixToParent"))) {
        applyTransformSample(payloadData);
        emitTransformHealth(false, sample.sampleId);
        return;
    }

    bool changed = false;

    if (payloadData.contains(QStringLiteral("navigating"))) {
        const bool navigating = payloadData.value(QStringLiteral("navigating")).toBool();
        if (m_navigating != navigating) {
            m_navigating = navigating;
            changed = true;
        }
    }

    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
    const bool hasPosition = extractPosition(payloadData, x, y, z);

    if (hasPosition) {
        m_currentPositionX = x;
        m_currentPositionY = y;
        m_currentPositionZ = z;
    }

    const QString status = payloadData.value(QStringLiteral("status")).toString();
    if (!status.isEmpty()) {
        if (m_navigationStatus != status) {
            m_navigationStatus = status;
            changed = true;
        }
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
    emitTransformHealth(true);
}

TransformNode* NavigationModuleLogicHandler::ensureTrackedTransformNode(
    SceneGraph* scene,
    const QString& remoteNodeId)
{
    if (!scene || remoteNodeId.isEmpty()) {
        return nullptr;
    }

    const NavigationTransformDescriptor* descriptor = findDescriptor(remoteNodeId);
    if (!descriptor) {
        return nullptr;
    }

    const QString localNodeId = m_localTransformNodeIdsByRemoteId.value(remoteNodeId);
    if (!localNodeId.isEmpty()) {
        if (auto* existing = scene->getNodeById<TransformNode>(localNodeId)) {
            configureTrackedTransformNode(existing, *descriptor);
            if (QString parentRemoteId = toQString(descriptor->parentRemoteId);
                parentRemoteId.isEmpty()) {
                existing->setParentTransform(QString());
            } else if (auto* parentNode = ensureTrackedTransformNode(scene, parentRemoteId)) {
                existing->setParentTransform(parentNode->getNodeId());
            }
            return existing;
        }
        m_localTransformNodeIdsByRemoteId.remove(remoteNodeId);
    }

    if (auto* existing = findTrackedTransformNode(scene, remoteNodeId)) {
        configureTrackedTransformNode(existing, *descriptor);
        m_localTransformNodeIdsByRemoteId.insert(remoteNodeId, existing->getNodeId());
        return existing;
    }

    auto* transformNode = new TransformNode(scene);
    configureTrackedTransformNode(transformNode, *descriptor);
    scene->addNode(transformNode);

    m_localTransformNodeIdsByRemoteId.insert(remoteNodeId, transformNode->getNodeId());
    if (const QString parentRemoteId = toQString(descriptor->parentRemoteId);
        !parentRemoteId.isEmpty()) {
        if (auto* parentNode = ensureTrackedTransformNode(scene, parentRemoteId)) {
            transformNode->setParentTransform(parentNode->getNodeId());
        }
    }

    return transformNode;
}

TransformNode* NavigationModuleLogicHandler::findTrackedTransformNode(
    SceneGraph* scene,
    const QString& remoteNodeId) const
{
    if (!scene) {
        return nullptr;
    }

    const QVector<TransformNode*> transformNodes = scene->getAllTransformNodes();
    for (TransformNode* transformNode : transformNodes) {
        if (transformNode &&
            transformNode->getAttribute(QStringLiteral("remoteTransformId")).toString() ==
                remoteNodeId) {
            return transformNode;
        }
    }

    return nullptr;
}

void NavigationModuleLogicHandler::applyTransformSample(const QVariantMap& payload)
{
    SceneGraph* scene = getSceneGraph();
    if (!scene) {
        return;
    }

    const QString remoteNodeId = payload.value(QStringLiteral("nodeId")).toString();
    auto* transformNode = ensureTrackedTransformNode(scene, remoteNodeId);
    if (!transformNode) {
        return;
    }

    const NavigationTransformDescriptor* descriptor = findDescriptor(remoteNodeId);
    if (descriptor) {
        configureTrackedTransformNode(transformNode, *descriptor);
        if (const QString parentRemoteId = toQString(descriptor->parentRemoteId);
            !parentRemoteId.isEmpty()) {
            if (auto* parentNode = ensureTrackedTransformNode(scene, parentRemoteId)) {
                transformNode->setParentTransform(parentNode->getNodeId());
            }
        } else {
            transformNode->setParentTransform(QString());
        }
    }

    double matrix[16];
    if (!extractMatrixToParent(payload, matrix)) {
        return;
    }

    transformNode->setMatrixTransformToParent(matrix);
    const qint64 timestampMs = payload.value(QStringLiteral("timestampMs")).toLongLong();
    m_lastSampleTimestampMsByRemoteId.insert(
        remoteNodeId,
        timestampMs > 0 ? timestampMs : QDateTime::currentMSecsSinceEpoch());
}

void NavigationModuleLogicHandler::emitNavigationState(const QString& sourceActionId,
                                                       const QString& sourceSampleId)
{
    QVariantMap payload;
    payload.insert(QStringLiteral("navigating"), m_navigating);
    payload.insert(QStringLiteral("status"), m_navigationStatus);
    payload.insert(QStringLiteral("x"), m_currentPositionX);
    payload.insert(QStringLiteral("y"), m_currentPositionY);
    payload.insert(QStringLiteral("z"), m_currentPositionZ);

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

void NavigationModuleLogicHandler::emitTransformHealth(bool force,
                                                       const QString& sourceSampleId)
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    if (!force && m_lastTransformHealthEmitMs > 0 &&
        (nowMs - m_lastTransformHealthEmitMs) < 33) {
        return;
    }

    m_lastTransformHealthEmitMs = nowMs;

    QVariantMap payload;
    payload.insert(QStringLiteral("eventName"),
                   QStringLiteral("navigation_transform_health"));
    payload.insert(QStringLiteral("transforms"),
                   buildTransformStatusList(m_lastSampleTimestampMsByRemoteId));
    payload.insert(QStringLiteral("timestampMs"), nowMs);
    payload.insert(QStringLiteral("navigating"), m_navigating);
    payload.insert(QStringLiteral("status"), m_navigationStatus);
    if (!sourceSampleId.isEmpty()) {
        payload.insert(QStringLiteral("sourceSampleId"), sourceSampleId);
    }

    LogicNotification notification = LogicNotification::create(
        LogicNotification::CustomEvent,
        LogicNotification::Shell,
        payload);
    emit logicNotification(notification);
}
