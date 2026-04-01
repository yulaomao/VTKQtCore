#pragma once

#include "NodeBase.h"
#include <QVector>

struct PointItem {
    QString pointId;
    QString label;
    double position[3] = {0, 0, 0};
    bool selectedFlag = false;
    bool visibleFlag = true;
    bool lockedFlag = false;
    QString associatedNodeId;
    double colorRGBA[4] = {-1, -1, -1, -1};
    double sizeValue = -1;
};

class PointNode : public NodeBase
{
    Q_OBJECT

public:
    explicit PointNode(QObject* parent = nullptr);
    ~PointNode() override = default;

    int addPoint(const PointItem& point);
    void insertPoint(int index, const PointItem& point);
    void removePoint(int index);
    void removeAllPoints();
    int getPointCount() const;

    const PointItem& getPointByIndex(int index) const;
    const PointItem* getPointById(const QString& pointId) const;

    void setPointPosition(int index, const double pos[3]);
    void getPointPosition(int index, double out[3]) const;

    void setPointLabel(int index, const QString& label);
    void setPointSelected(int index, bool selected);
    void setPointLocked(int index, bool locked);

    void setParentTransform(const QString& transformId);
    QString getParentTransform() const;

    void setPointColor(int index, const double rgba[4]);
    void getPointColor(int index, double out[4]) const;

    void setPointSize(int index, double size);
    double getPointSize(int index) const;

    void setDefaultPointColor(const double rgba[4]);
    void getDefaultPointColor(double out[4]) const;

    void setDefaultPointSize(double size);
    double getDefaultPointSize() const;

    void setShowPointLabel(bool show);
    bool isShowPointLabel() const;

    void setPointLabelFormat(const QString& format);
    QString getPointLabelFormat() const;

    void setSelectedPointIndex(int index);
    int getSelectedPointIndex() const;

    void setPointRole(const QString& role);
    QString getPointRole() const;

private:
    QVector<PointItem> m_controlPoints;
    QString m_pointLabelFormat = QStringLiteral("%1");
    int m_selectedPointIndex = -1;
    QString m_pointRole;
    QString m_parentTransformId;
    double m_defaultPointColor[4] = {1.0, 0.0, 0.0, 1.0};
    double m_defaultPointSize = 5.0;
    bool m_showPointLabelFlag = true;
};
