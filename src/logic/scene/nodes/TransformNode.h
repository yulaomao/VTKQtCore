#pragma once

#include "NodeBase.h"

class TransformNode : public NodeBase
{
    Q_OBJECT

public:
    explicit TransformNode(QObject* parent = nullptr);
    ~TransformNode() override = default;

    void setMatrixTransformToParent(const double m[16]);
    void getMatrixTransformToParent(double out[16]) const;

    void getInverseMatrix(double out[16]);

    void multiplyBy(const double m[16]);
    void inverse();
    bool isIdentity() const;

    void transformPoint(const double in[3], double out[3]) const;
    void transformVector(const double in[3], double out[3]) const;

    void setTransformKind(const QString& kind);
    QString getTransformKind() const;

    void setSourceSpace(const QString& space);
    QString getSourceSpace() const;

    void setTargetSpace(const QString& space);
    QString getTargetSpace() const;

    void setParentTransform(const QString& transformId);
    QString getParentTransform() const;

    void setShowAxes(bool show);
    bool isShowAxes() const;

    void setAxesLength(double length);
    double getAxesLength() const;

    void setAxesColorX(const double rgba[4]);
    void getAxesColorX(double out[4]) const;

    void setAxesColorY(const double rgba[4]);
    void getAxesColorY(double out[4]) const;

    void setAxesColorZ(const double rgba[4]);
    void getAxesColorZ(double out[4]) const;

private:
    void invalidateInverseCache();

    double m_matrixData[16];
    bool m_inverseDirtyFlag = true;
    double m_cachedInverse[16];
    QString m_transformKind;
    QString m_sourceSpaceName;
    QString m_targetSpaceName;
    QString m_parentTransformId;
    bool m_showAxesFlag = false;
    double m_axesLengthValue = 50.0;
    double m_axesColorX[4] = {1, 0, 0, 1};
    double m_axesColorY[4] = {0, 1, 0, 1};
    double m_axesColorZ[4] = {0, 0, 1, 1};
};
