#pragma once

#include "NodeBase.h"
#include <QVector>
#include <array>
#include <cmath>

class LineNode : public NodeBase
{
    Q_OBJECT

public:
    explicit LineNode(QObject* parent = nullptr);
    ~LineNode() override = default;

    void setPolyline(const QVector<std::array<double, 3>>& points);
    void appendVertex(const std::array<double, 3>& vertex);
    void removeVertex(int index);
    void clearVertices();
    int getVertexCount() const;
    std::array<double, 3> getVertex(int index) const;

    void setClosed(bool closed);
    bool isClosed() const;

    void recalculateLength();
    double getLength() const;

    void setParentTransform(const QString& transformId);
    QString getParentTransform() const;

    void setColor(const double rgba[4]);
    void getColor(double out[4]) const;

    void setOpacity(double opacity);
    double getOpacity() const;

    void setLineWidth(double width);
    double getLineWidth() const;

    void setRenderMode(const QString& mode);
    QString getRenderMode() const;

    void setDashed(bool dashed);
    bool isDashed() const;

    void setLineRole(const QString& role);
    QString getLineRole() const;

private:
    QVector<std::array<double, 3>> m_polylinePoints;
    bool m_closedFlag = false;
    QString m_lineRole;
    double m_cachedLength = 0.0;
    double m_colorRGBA[4] = {0.0, 1.0, 0.0, 1.0};
    double m_opacityValue = 1.0;
    double m_lineWidthValue = 2.0;
    QString m_renderMode = QStringLiteral("surface");
    bool m_dashedFlag = false;
};
