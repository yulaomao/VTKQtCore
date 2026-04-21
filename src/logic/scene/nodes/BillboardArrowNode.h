#pragma once

#include "NodeBase.h"

#include <array>

class BillboardArrowNode : public NodeBase
{
    Q_OBJECT

public:
    explicit BillboardArrowNode(QObject* parent = nullptr);
    ~BillboardArrowNode() override = default;

    void setTipPoint(const std::array<double, 3>& tipPoint);
    std::array<double, 3> getTipPoint() const;

    void setParentTransform(const QString& transformId);
    QString getParentTransform() const;

    void setDirection(const QString& direction);
    QString getDirection() const;

    void setFollowCameraRotation(bool followCameraRotation);
    bool isFollowCameraRotation() const;

    void setColor(const double rgba[4]);
    void getColor(double out[4]) const;

    void setOpacity(double opacity);
    double getOpacity() const;

    void setSize(double shaftLength,
                 double shaftWidth,
                 double headLength,
                 double headWidth);
    double getShaftLength() const;
    double getShaftWidth() const;
    double getHeadLength() const;
    double getHeadWidth() const;

    void setArrowRole(const QString& role);
    QString getArrowRole() const;

private:
    static QString normalizeDirection(const QString& direction);

    std::array<double, 3> m_tipPoint = {0.0, 0.0, 0.0};
    QString m_direction = QStringLiteral("up");
    bool m_followCameraRotation = false;
    QString m_arrowRole;
    double m_colorRGBA[4] = {0.95, 0.85, 0.2, 1.0};
    double m_opacityValue = 1.0;
    double m_shaftLengthValue = 30.0;
    double m_shaftWidthValue = 3.0;
    double m_headLengthValue = 14.0;
    double m_headWidthValue = 18.0;
};