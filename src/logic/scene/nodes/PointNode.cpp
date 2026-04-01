#include "PointNode.h"

#include "../SceneGraph.h"

#include <QUuid>

PointNode::PointNode(QObject* parent)
    : NodeBase(QStringLiteral("PointNode"), parent)
{
    DisplayTarget dt;
    dt.visible = true;
    dt.layer = 3;
    setDefaultDisplayTarget(dt);
}

int PointNode::addPoint(const PointItem& point)
{
    PointItem p = point;
    if (p.pointId.isEmpty()) {
        p.pointId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    m_controlPoints.append(p);
    touchModified();
    return m_controlPoints.size() - 1;
}

void PointNode::insertPoint(int index, const PointItem& point)
{
    if (index < 0 || index > m_controlPoints.size()) {
        return;
    }
    PointItem p = point;
    if (p.pointId.isEmpty()) {
        p.pointId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    m_controlPoints.insert(index, p);
    touchModified();
}

void PointNode::removePoint(int index)
{
    if (index < 0 || index >= m_controlPoints.size()) {
        return;
    }
    m_controlPoints.removeAt(index);
    if (m_selectedPointIndex >= m_controlPoints.size()) {
        m_selectedPointIndex = m_controlPoints.isEmpty() ? -1 : m_controlPoints.size() - 1;
    }
    touchModified();
}

void PointNode::removeAllPoints()
{
    if (m_controlPoints.isEmpty()) {
        return;
    }
    m_controlPoints.clear();
    m_selectedPointIndex = -1;
    touchModified();
}

int PointNode::getPointCount() const
{
    return m_controlPoints.size();
}

const PointItem& PointNode::getPointByIndex(int index) const
{
    return m_controlPoints.at(index);
}

const PointItem* PointNode::getPointById(const QString& pointId) const
{
    for (const auto& pt : m_controlPoints) {
        if (pt.pointId == pointId) {
            return &pt;
        }
    }
    return nullptr;
}

void PointNode::setPointPosition(int index, const double pos[3])
{
    if (index < 0 || index >= m_controlPoints.size()) {
        return;
    }
    auto& pt = m_controlPoints[index];
    pt.position[0] = pos[0];
    pt.position[1] = pos[1];
    pt.position[2] = pos[2];
    touchModified();
}

void PointNode::getPointPosition(int index, double out[3]) const
{
    if (index < 0 || index >= m_controlPoints.size()) {
        out[0] = out[1] = out[2] = 0.0;
        return;
    }
    const auto& pt = m_controlPoints.at(index);
    out[0] = pt.position[0];
    out[1] = pt.position[1];
    out[2] = pt.position[2];
}

void PointNode::setPointLabel(int index, const QString& label)
{
    if (index < 0 || index >= m_controlPoints.size()) {
        return;
    }
    m_controlPoints[index].label = label;
    touchModified();
}

void PointNode::setPointSelected(int index, bool selected)
{
    if (index < 0 || index >= m_controlPoints.size()) {
        return;
    }
    m_controlPoints[index].selectedFlag = selected;
    if (selected) {
        m_selectedPointIndex = index;
    } else if (m_selectedPointIndex == index) {
        m_selectedPointIndex = -1;
    }
    emitEvent(NodeEventType::DisplayChanged);
}

void PointNode::setPointLocked(int index, bool locked)
{
    if (index < 0 || index >= m_controlPoints.size()) {
        return;
    }
    m_controlPoints[index].lockedFlag = locked;
    touchModified();
}

void PointNode::setParentTransform(const QString& transformId)
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

QString PointNode::getParentTransform() const
{
    return getFirstReference(NodeBase::parentTransformReferenceRole());
}

void PointNode::setPointColor(int index, const double rgba[4])
{
    if (index < 0 || index >= m_controlPoints.size()) {
        return;
    }
    auto& pt = m_controlPoints[index];
    for (int i = 0; i < 4; ++i) {
        pt.colorRGBA[i] = rgba[i];
    }
    emitEvent(NodeEventType::DisplayChanged);
}

void PointNode::getPointColor(int index, double out[4]) const
{
    if (index < 0 || index >= m_controlPoints.size()) {
        for (int i = 0; i < 4; ++i) {
            out[i] = m_defaultPointColor[i];
        }
        return;
    }
    const auto& pt = m_controlPoints.at(index);
    for (int i = 0; i < 4; ++i) {
        out[i] = (pt.colorRGBA[i] < 0) ? m_defaultPointColor[i] : pt.colorRGBA[i];
    }
}

void PointNode::setPointSize(int index, double size)
{
    if (index < 0 || index >= m_controlPoints.size()) {
        return;
    }
    m_controlPoints[index].sizeValue = size;
    emitEvent(NodeEventType::DisplayChanged);
}

double PointNode::getPointSize(int index) const
{
    if (index < 0 || index >= m_controlPoints.size()) {
        return m_defaultPointSize;
    }
    double sz = m_controlPoints.at(index).sizeValue;
    return (sz < 0) ? m_defaultPointSize : sz;
}

void PointNode::setDefaultPointColor(const double rgba[4])
{
    for (int i = 0; i < 4; ++i) {
        m_defaultPointColor[i] = rgba[i];
    }
    emitEvent(NodeEventType::DisplayChanged);
}

void PointNode::getDefaultPointColor(double out[4]) const
{
    for (int i = 0; i < 4; ++i) {
        out[i] = m_defaultPointColor[i];
    }
}

void PointNode::setDefaultPointSize(double size)
{
    m_defaultPointSize = size;
    emitEvent(NodeEventType::DisplayChanged);
}

double PointNode::getDefaultPointSize() const
{
    return m_defaultPointSize;
}

void PointNode::setShowPointLabel(bool show)
{
    if (m_showPointLabelFlag != show) {
        m_showPointLabelFlag = show;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

bool PointNode::isShowPointLabel() const
{
    return m_showPointLabelFlag;
}

void PointNode::setPointLabelFormat(const QString& format)
{
    if (m_pointLabelFormat != format) {
        m_pointLabelFormat = format;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

QString PointNode::getPointLabelFormat() const
{
    return m_pointLabelFormat;
}

void PointNode::setSelectedPointIndex(int index)
{
    if (m_selectedPointIndex != index) {
        m_selectedPointIndex = index;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

int PointNode::getSelectedPointIndex() const
{
    return m_selectedPointIndex;
}

void PointNode::setPointRole(const QString& role)
{
    if (m_pointRole != role) {
        m_pointRole = role;
        touchModified();
    }
}

QString PointNode::getPointRole() const
{
    return m_pointRole;
}
