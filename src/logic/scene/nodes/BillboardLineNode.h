#pragma once

#include "NodeBase.h"

#include <array>

class BillboardLineNode : public NodeBase
{
    Q_OBJECT

public:
    explicit BillboardLineNode(QObject* parent = nullptr);
    ~BillboardLineNode() override = default;

    void setStartPoint(const std::array<double, 3>& startPoint);
    std::array<double, 3> getStartPoint() const;

    void setEndPoint(const std::array<double, 3>& endPoint);
    std::array<double, 3> getEndPoint() const;

    void setEndpoints(const std::array<double, 3>& startPoint,
                      const std::array<double, 3>& endPoint);

    void setParentTransform(const QString& transformId);
    QString getParentTransform() const;

    void setColor(const double rgba[4]);
    void getColor(double out[4]) const;

    void setOpacity(double opacity);
    double getOpacity() const;

    void setLineWidth(double width);
    double getLineWidth() const;

    void setDashed(bool dashed);
    bool isDashed() const;

    void setDashPattern(double dashLength, double gapLength);
    double getDashLength() const;
    double getGapLength() const;

    void setLineRole(const QString& role);
    QString getLineRole() const;

private:
    std::array<double, 3> m_startPoint = {0.0, 0.0, 0.0};
    std::array<double, 3> m_endPoint = {1.0, 0.0, 0.0};
    QString m_lineRole;
    double m_colorRGBA[4] = {0.94, 0.43, 0.23, 1.0};
    double m_opacityValue = 1.0;
    double m_lineWidthValue = 8.0;
    bool m_dashedFlag = false;
    double m_dashLengthValue = 0.5;
    double m_gapLengthValue = 0.25;
};