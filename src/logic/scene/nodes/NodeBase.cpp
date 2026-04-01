#include "NodeBase.h"

namespace {

const QString kParentTransformReferenceRole = QStringLiteral("parentTransform");

}

NodeBase::NodeBase(const QString& nodeTagName, QObject* parent)
    : QObject(parent)
    , m_nodeId(QUuid::createUuid().toString(QUuid::WithoutBraces))
    , m_nodeTagName(nodeTagName)
{
}

const QString& NodeBase::parentTransformReferenceRole()
{
    return kParentTransformReferenceRole;
}

QString NodeBase::getNodeId() const
{
    return m_nodeId;
}

QString NodeBase::getNodeTagName() const
{
    return m_nodeTagName;
}

void NodeBase::setName(const QString& name)
{
    if (m_name != name) {
        m_name = name;
        touchModified();
    }
}

QString NodeBase::getName() const
{
    return m_name;
}

void NodeBase::setDescription(const QString& description)
{
    if (m_description != description) {
        m_description = description;
        touchModified();
    }
}

QString NodeBase::getDescription() const
{
    return m_description;
}

int NodeBase::getVersion() const
{
    return m_version;
}

bool NodeBase::isDirty() const
{
    return m_dirtyFlag;
}

void NodeBase::setDirty(bool dirty)
{
    m_dirtyFlag = dirty;
}

void NodeBase::setAttribute(const QString& key, const QVariant& value)
{
    m_attributeMap[key] = value;
    touchModified();
}

QVariant NodeBase::getAttribute(const QString& key, const QVariant& defaultValue) const
{
    return m_attributeMap.value(key, defaultValue);
}

void NodeBase::removeAttribute(const QString& key)
{
    if (m_attributeMap.remove(key)) {
        touchModified();
    }
}

QVariantMap NodeBase::getAttributes() const
{
    return m_attributeMap;
}

void NodeBase::setReference(const QString& role, const QString& nodeId)
{
    if (role.isEmpty()) {
        return;
    }

    if (nodeId.isEmpty()) {
        if (m_referenceMap.remove(role) > 0) {
            emitEvent(NodeEventType::ReferenceChanged);
        }
        return;
    }

    const QStringList nextValue{nodeId};
    if (m_referenceMap.value(role) == nextValue) {
        return;
    }

    m_referenceMap[role] = nextValue;
    emitEvent(NodeEventType::ReferenceChanged);
}

void NodeBase::addReference(const QString& role, const QString& nodeId)
{
    if (role.isEmpty() || nodeId.isEmpty()) {
        return;
    }

    QStringList& refs = m_referenceMap[role];
    if (!refs.contains(nodeId)) {
        refs.append(nodeId);
        emitEvent(NodeEventType::ReferenceChanged);
    }
}

void NodeBase::removeReference(const QString& role, const QString& nodeId)
{
    if (m_referenceMap.contains(role)) {
        m_referenceMap[role].removeAll(nodeId);
        if (m_referenceMap[role].isEmpty()) {
            m_referenceMap.remove(role);
        }
        emitEvent(NodeEventType::ReferenceChanged);
    }
}

QStringList NodeBase::getReferences(const QString& role) const
{
    return m_referenceMap.value(role);
}

QString NodeBase::getFirstReference(const QString& role) const
{
    const QStringList refs = getReferences(role);
    return refs.isEmpty() ? QString() : refs.first();
}

void NodeBase::startBatchModify()
{
    m_batchModifyDepth++;
}

void NodeBase::endBatchModify()
{
    if (m_batchModifyDepth > 0) {
        m_batchModifyDepth--;
    }
    if (m_batchModifyDepth == 0 && !m_pendingEvents.isEmpty()) {
        QSet<NodeEventType> pending = m_pendingEvents;
        m_pendingEvents.clear();
        for (auto type : pending) {
            emit nodeEvent(m_nodeId, type);
        }
        emit nodeEvent(m_nodeId, NodeEventType::NodeModified);
    }
}

void NodeBase::touchModified()
{
    m_version++;
    m_dirtyFlag = true;
    emitEvent(NodeEventType::ContentModified);
}

void NodeBase::copyContentFrom(const NodeBase* other)
{
    if (!other) {
        return;
    }
    startBatchModify();
    m_name = other->m_name;
    m_description = other->m_description;
    m_attributeMap = other->m_attributeMap;
    m_referenceMap = other->m_referenceMap;
    m_defaultDisplayTarget = other->m_defaultDisplayTarget;
    m_windowDisplayOverrides = other->m_windowDisplayOverrides;
    m_opacity = other->m_opacity;
    for (int i = 0; i < 4; ++i) {
        m_colorRGBA[i] = other->m_colorRGBA[i];
    }
    m_version++;
    m_dirtyFlag = true;
    m_pendingEvents.insert(NodeEventType::ContentModified);
    endBatchModify();
}

void NodeBase::setDefaultDisplayTarget(const DisplayTarget& target)
{
    m_defaultDisplayTarget = target;
    emitEvent(NodeEventType::DisplayChanged);
}

DisplayTarget NodeBase::getDefaultDisplayTarget() const
{
    return m_defaultDisplayTarget;
}

void NodeBase::setWindowDisplayTarget(const QString& windowId, const DisplayTarget& target)
{
    m_windowDisplayOverrides[windowId] = target;
    emitEvent(NodeEventType::DisplayChanged);
}

void NodeBase::removeWindowDisplayTarget(const QString& windowId)
{
    if (m_windowDisplayOverrides.remove(windowId)) {
        emitEvent(NodeEventType::DisplayChanged);
    }
}

DisplayTarget NodeBase::getDisplayTargetForWindow(const QString& windowId) const
{
    if (m_windowDisplayOverrides.contains(windowId)) {
        return m_windowDisplayOverrides.value(windowId);
    }
    return m_defaultDisplayTarget;
}

void NodeBase::setVisibility(bool visible)
{
    if (m_defaultDisplayTarget.visible != visible) {
        m_defaultDisplayTarget.visible = visible;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

bool NodeBase::isVisible() const
{
    return m_defaultDisplayTarget.visible;
}

void NodeBase::setOpacity(double opacity)
{
    if (m_opacity != opacity) {
        m_opacity = opacity;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

double NodeBase::getOpacity() const
{
    return m_opacity;
}

void NodeBase::setColor(double r, double g, double b, double a)
{
    m_colorRGBA[0] = r;
    m_colorRGBA[1] = g;
    m_colorRGBA[2] = b;
    m_colorRGBA[3] = a;
    emitEvent(NodeEventType::DisplayChanged);
}

void NodeBase::getColor(double outRGBA[4]) const
{
    for (int i = 0; i < 4; ++i) {
        outRGBA[i] = m_colorRGBA[i];
    }
}

void NodeBase::emitEvent(NodeEventType type)
{
    if (m_batchModifyDepth > 0) {
        m_pendingEvents.insert(type);
        return;
    }
    emit nodeEvent(m_nodeId, type);
}
