#include "BillboardArrowNode.h"

#include "../SceneGraph.h"

#include <algorithm>

namespace {

bool arePointsEqual(const std::array<double, 3>& lhs, const std::array<double, 3>& rhs)
{
    return lhs == rhs;
}

}

BillboardArrowNode::BillboardArrowNode(QObject* parent)
    : NodeBase(QStringLiteral("BillboardArrowNode"), parent)
{
    DisplayTarget dt;
    dt.visible = true;
    dt.layer = 3;
    setDefaultDisplayTarget(dt);
}

void BillboardArrowNode::setTipPoint(const std::array<double, 3>& tipPoint)
{
    if (arePointsEqual(m_tipPoint, tipPoint)) {
        return;
    }

    m_tipPoint = tipPoint;
    touchModified();
}

std::array<double, 3> BillboardArrowNode::getTipPoint() const
{
    return m_tipPoint;
}

void BillboardArrowNode::setParentTransform(const QString& transformId)
{
    SceneGraph* scene = qobject_cast<SceneGraph*>(parent());
    const QString currentId = getParentTransform();
    if (currentId == transformId) {
        return;
    }

    if (transformId == getNodeId()) {
        return;
    }

    if (scene && !scene->canAssignParentTransform(getNodeId(), transformId)) {
        return;
    }

    setReference(NodeBase::parentTransformReferenceRole(), transformId);
    emitEvent(NodeEventType::TransformChanged);
}

QString BillboardArrowNode::getParentTransform() const
{
    return getFirstReference(NodeBase::parentTransformReferenceRole());
}

QString BillboardArrowNode::normalizeDirection(const QString& direction)
{
    const QString normalized = direction.trimmed().toLower();
    return normalized == QStringLiteral("down") ? QStringLiteral("down") : QStringLiteral("up");
}

void BillboardArrowNode::setDirection(const QString& direction)
{
    const QString normalized = normalizeDirection(direction);
    if (m_direction != normalized) {
        m_direction = normalized;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

QString BillboardArrowNode::getDirection() const
{
    return m_direction;
}

void BillboardArrowNode::setFollowCameraRotation(bool followCameraRotation)
{
    if (m_followCameraRotation != followCameraRotation) {
        m_followCameraRotation = followCameraRotation;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

bool BillboardArrowNode::isFollowCameraRotation() const
{
    return m_followCameraRotation;
}

void BillboardArrowNode::setColor(const double rgba[4])
{
    for (int i = 0; i < 4; ++i) {
        m_colorRGBA[i] = rgba[i];
    }
    emitEvent(NodeEventType::DisplayChanged);
}

void BillboardArrowNode::getColor(double out[4]) const
{
    for (int i = 0; i < 4; ++i) {
        out[i] = m_colorRGBA[i];
    }
}

void BillboardArrowNode::setOpacity(double opacity)
{
    if (m_opacityValue != opacity) {
        m_opacityValue = opacity;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

double BillboardArrowNode::getOpacity() const
{
    return m_opacityValue;
}

void BillboardArrowNode::setSize(double shaftLength,
                                 double shaftWidth,
                                 double headLength,
                                 double headWidth)
{
    const double nextShaftLength = std::max(shaftLength, 0.0);
    const double nextShaftWidth = std::max(shaftWidth, 1.0);
    const double nextHeadLength = std::max(headLength, 1.0);
    const double nextHeadWidth = std::max(headWidth, 1.0);

    if (m_shaftLengthValue == nextShaftLength &&
        m_shaftWidthValue == nextShaftWidth &&
        m_headLengthValue == nextHeadLength &&
        m_headWidthValue == nextHeadWidth) {
        return;
    }

    m_shaftLengthValue = nextShaftLength;
    m_shaftWidthValue = nextShaftWidth;
    m_headLengthValue = nextHeadLength;
    m_headWidthValue = nextHeadWidth;
    emitEvent(NodeEventType::DisplayChanged);
}

double BillboardArrowNode::getShaftLength() const
{
    return m_shaftLengthValue;
}

double BillboardArrowNode::getShaftWidth() const
{
    return m_shaftWidthValue;
}

double BillboardArrowNode::getHeadLength() const
{
    return m_headLengthValue;
}

double BillboardArrowNode::getHeadWidth() const
{
    return m_headWidthValue;
}

void BillboardArrowNode::setArrowRole(const QString& role)
{
    if (m_arrowRole != role) {
        m_arrowRole = role;
        touchModified();
    }
}

QString BillboardArrowNode::getArrowRole() const
{
    return m_arrowRole;
}