#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QMap>
#include <QStringList>
#include <QUuid>
#include <QSet>

struct DisplayTarget {
    bool visible = true;
    int layer = 1;
};

enum class NodeEventType {
    NodeModified,
    ContentModified,
    ReferenceChanged,
    DisplayChanged,
    TransformChanged
};

class NodeBase : public QObject
{
    Q_OBJECT

public:
    explicit NodeBase(const QString& nodeTagName, QObject* parent = nullptr);
    ~NodeBase() override = default;

    static const QString& parentTransformReferenceRole();

    QString getNodeId() const;
    QString getNodeTagName() const;

    void setName(const QString& name);
    QString getName() const;

    void setDescription(const QString& description);
    QString getDescription() const;

    int getVersion() const;
    bool isDirty() const;
    void setDirty(bool dirty);

    void setAttribute(const QString& key, const QVariant& value);
    QVariant getAttribute(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void removeAttribute(const QString& key);
    QVariantMap getAttributes() const;

    void setReference(const QString& role, const QString& nodeId);
    void addReference(const QString& role, const QString& nodeId);
    void removeReference(const QString& role, const QString& nodeId);
    QStringList getReferences(const QString& role) const;
    QString getFirstReference(const QString& role) const;

    void startBatchModify();
    void endBatchModify();

    void touchModified();

    void copyContentFrom(const NodeBase* other);

    void setDefaultDisplayTarget(const DisplayTarget& target);
    DisplayTarget getDefaultDisplayTarget() const;
    void setWindowDisplayTarget(const QString& windowId, const DisplayTarget& target);
    void removeWindowDisplayTarget(const QString& windowId);
    DisplayTarget getDisplayTargetForWindow(const QString& windowId) const;

    void setVisibility(bool visible);
    bool isVisible() const;

    void setOpacity(double opacity);
    double getOpacity() const;

    void setColor(double r, double g, double b, double a = 1.0);
    void getColor(double outRGBA[4]) const;

signals:
    void nodeEvent(const QString& nodeId, NodeEventType eventType);

protected:
    void emitEvent(NodeEventType type);

private:
    const QString m_nodeId;
    const QString m_nodeTagName;
    QString m_name;
    QString m_description;
    int m_version = 0;
    bool m_dirtyFlag = false;
    QVariantMap m_attributeMap;
    QMap<QString, QStringList> m_referenceMap;
    DisplayTarget m_defaultDisplayTarget;
    QMap<QString, DisplayTarget> m_windowDisplayOverrides;
    int m_batchModifyDepth = 0;
    QSet<NodeEventType> m_pendingEvents;
    double m_opacity = 1.0;
    double m_colorRGBA[4] = {1.0, 1.0, 1.0, 1.0};
};
