#include "LineNode.h"

#include "../SceneGraph.h"

LineNode::LineNode(QObject* parent)
    : NodeBase(QStringLiteral("LineNode"), parent)
{
    DisplayTarget dt;
    dt.visible = true;
    dt.layer = 3;
    setDefaultDisplayTarget(dt);
}

void LineNode::setPolyline(const QVector<std::array<double, 3>>& points)
{
    m_polylinePoints = points;
    recalculateLength();
    touchModified();
}

void LineNode::appendVertex(const std::array<double, 3>& vertex)
{
    m_polylinePoints.append(vertex);
    recalculateLength();
    touchModified();
}

void LineNode::removeVertex(int index)
{
    if (index < 0 || index >= m_polylinePoints.size()) {
        return;
    }
    m_polylinePoints.removeAt(index);
    recalculateLength();
    touchModified();
}

void LineNode::clearVertices()
{
    if (m_polylinePoints.isEmpty()) {
        return;
    }
    m_polylinePoints.clear();
    m_cachedLength = 0.0;
    touchModified();
}

int LineNode::getVertexCount() const
{
    return m_polylinePoints.size();
}

std::array<double, 3> LineNode::getVertex(int index) const
{
    if (index < 0 || index >= m_polylinePoints.size()) {
        return {0.0, 0.0, 0.0};
    }
    return m_polylinePoints.at(index);
}

void LineNode::setClosed(bool closed)
{
    if (m_closedFlag != closed) {
        m_closedFlag = closed;
        recalculateLength();
        touchModified();
    }
}

bool LineNode::isClosed() const
{
    return m_closedFlag;
}

void LineNode::recalculateLength()
{
    m_cachedLength = 0.0;
    if (m_polylinePoints.size() < 2) {
        return;
    }
    for (int i = 1; i < m_polylinePoints.size(); ++i) {
        const auto& a = m_polylinePoints[i - 1];
        const auto& b = m_polylinePoints[i];
        double dx = b[0] - a[0];
        double dy = b[1] - a[1];
        double dz = b[2] - a[2];
        m_cachedLength += std::sqrt(dx * dx + dy * dy + dz * dz);
    }
    if (m_closedFlag && m_polylinePoints.size() > 2) {
        const auto& first = m_polylinePoints.first();
        const auto& last = m_polylinePoints.last();
        double dx = first[0] - last[0];
        double dy = first[1] - last[1];
        double dz = first[2] - last[2];
        m_cachedLength += std::sqrt(dx * dx + dy * dy + dz * dz);
    }
}

double LineNode::getLength() const
{
    return m_cachedLength;
}

void LineNode::setParentTransform(const QString& transformId)
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

QString LineNode::getParentTransform() const
{
    return getFirstReference(NodeBase::parentTransformReferenceRole());
}

void LineNode::setColor(const double rgba[4])
{
    for (int i = 0; i < 4; ++i) {
        m_colorRGBA[i] = rgba[i];
    }
    emitEvent(NodeEventType::DisplayChanged);
}

void LineNode::getColor(double out[4]) const
{
    for (int i = 0; i < 4; ++i) {
        out[i] = m_colorRGBA[i];
    }
}

void LineNode::setOpacity(double opacity)
{
    if (m_opacityValue != opacity) {
        m_opacityValue = opacity;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

double LineNode::getOpacity() const
{
    return m_opacityValue;
}

void LineNode::setLineWidth(double width)
{
    if (m_lineWidthValue != width) {
        m_lineWidthValue = width;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

double LineNode::getLineWidth() const
{
    return m_lineWidthValue;
}

void LineNode::setRenderMode(const QString& mode)
{
    if (m_renderMode != mode) {
        m_renderMode = mode;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

QString LineNode::getRenderMode() const
{
    return m_renderMode;
}

void LineNode::setDashed(bool dashed)
{
    if (m_dashedFlag != dashed) {
        m_dashedFlag = dashed;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

bool LineNode::isDashed() const
{
    return m_dashedFlag;
}

void LineNode::setLineRole(const QString& role)
{
    if (m_lineRole != role) {
        m_lineRole = role;
        touchModified();
    }
}

QString LineNode::getLineRole() const
{
    return m_lineRole;
}
