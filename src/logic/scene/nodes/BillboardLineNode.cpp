#include "BillboardLineNode.h"

#include "../SceneGraph.h"

#include <algorithm>

namespace {

bool arePointsEqual(const std::array<double, 3>& lhs, const std::array<double, 3>& rhs)
{
    return lhs == rhs;
}

}

BillboardLineNode::BillboardLineNode(QObject* parent)
    : NodeBase(QStringLiteral("BillboardLineNode"), parent)
{
    DisplayTarget dt;
    dt.visible = true;
    dt.layer = 3;
    setDefaultDisplayTarget(dt);
}

void BillboardLineNode::setStartPoint(const std::array<double, 3>& startPoint)
{
    if (arePointsEqual(m_startPoint, startPoint)) {
        return;
    }

    m_startPoint = startPoint;
    touchModified();
}

std::array<double, 3> BillboardLineNode::getStartPoint() const
{
    return m_startPoint;
}

void BillboardLineNode::setEndPoint(const std::array<double, 3>& endPoint)
{
    if (arePointsEqual(m_endPoint, endPoint)) {
        return;
    }

    m_endPoint = endPoint;
    touchModified();
}

std::array<double, 3> BillboardLineNode::getEndPoint() const
{
    return m_endPoint;
}

void BillboardLineNode::setEndpoints(const std::array<double, 3>& startPoint,
                                     const std::array<double, 3>& endPoint)
{
    if (arePointsEqual(m_startPoint, startPoint) && arePointsEqual(m_endPoint, endPoint)) {
        return;
    }

    m_startPoint = startPoint;
    m_endPoint = endPoint;
    touchModified();
}

void BillboardLineNode::setParentTransform(const QString& transformId)
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

QString BillboardLineNode::getParentTransform() const
{
    return getFirstReference(NodeBase::parentTransformReferenceRole());
}

void BillboardLineNode::setColor(const double rgba[4])
{
    for (int i = 0; i < 4; ++i) {
        m_colorRGBA[i] = rgba[i];
    }
    emitEvent(NodeEventType::DisplayChanged);
}

void BillboardLineNode::getColor(double out[4]) const
{
    for (int i = 0; i < 4; ++i) {
        out[i] = m_colorRGBA[i];
    }
}

void BillboardLineNode::setOpacity(double opacity)
{
    if (m_opacityValue != opacity) {
        m_opacityValue = opacity;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

double BillboardLineNode::getOpacity() const
{
    return m_opacityValue;
}

void BillboardLineNode::setLineWidth(double width)
{
    if (m_lineWidthValue != width) {
        m_lineWidthValue = width;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

double BillboardLineNode::getLineWidth() const
{
    return m_lineWidthValue;
}

void BillboardLineNode::setDashed(bool dashed)
{
    if (m_dashedFlag != dashed) {
        m_dashedFlag = dashed;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

bool BillboardLineNode::isDashed() const
{
    return m_dashedFlag;
}

void BillboardLineNode::setDashPattern(double dashLength, double gapLength)
{
    const double clampedDashLength = std::max(dashLength, 0.001);
    const double clampedGapLength = std::max(gapLength, 0.0);
    if (m_dashLengthValue == clampedDashLength && m_gapLengthValue == clampedGapLength) {
        return;
    }

    m_dashLengthValue = clampedDashLength;
    m_gapLengthValue = clampedGapLength;
    emitEvent(NodeEventType::DisplayChanged);
}

double BillboardLineNode::getDashLength() const
{
    return m_dashLengthValue;
}

double BillboardLineNode::getGapLength() const
{
    return m_gapLengthValue;
}

void BillboardLineNode::setLineRole(const QString& role)
{
    if (m_lineRole != role) {
        m_lineRole = role;
        touchModified();
    }
}

QString BillboardLineNode::getLineRole() const
{
    return m_lineRole;
}