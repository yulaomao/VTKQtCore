#include "TransformNode.h"

#include "../SceneGraph.h"

#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <cstring>
#include <cmath>

namespace {

void setIdentity(double m[16])
{
    std::memset(m, 0, 16 * sizeof(double));
    m[0] = m[5] = m[10] = m[15] = 1.0;
}

bool matricesEqual(const double lhs[16], const double rhs[16], double eps = 1e-12)
{
    for (int i = 0; i < 16; ++i) {
        if (std::fabs(lhs[i] - rhs[i]) > eps) {
            return false;
        }
    }
    return true;
}

} // anonymous namespace

TransformNode::TransformNode(QObject* parent)
    : NodeBase(QStringLiteral("TransformNode"), parent)
{
    setIdentity(m_matrixData);
    setIdentity(m_cachedInverse);

    DisplayTarget dt;
    dt.visible = true;
    dt.layer = 3;
    setDefaultDisplayTarget(dt);
}

void TransformNode::setMatrixTransformToParent(const double m[16])
{
    if (matricesEqual(m_matrixData, m)) {
        return;
    }

    std::memcpy(m_matrixData, m, 16 * sizeof(double));
    invalidateInverseCache();
    touchModified();
    emitEvent(NodeEventType::TransformChanged);
}

void TransformNode::getMatrixTransformToParent(double out[16]) const
{
    std::memcpy(out, m_matrixData, 16 * sizeof(double));
}

void TransformNode::getInverseMatrix(double out[16])
{
    if (m_inverseDirtyFlag) {
        vtkNew<vtkMatrix4x4> mat;
        // vtkMatrix4x4 uses row-major Element[i][j], but we store column-major.
        // VTK's DeepCopy from double[16] treats it as row-major, so we transpose.
        double rowMajor[16];
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                rowMajor[r * 4 + c] = m_matrixData[c * 4 + r];
            }
        }
        mat->DeepCopy(rowMajor);

        vtkNew<vtkMatrix4x4> inv;
        vtkMatrix4x4::Invert(mat, inv);

        // Convert back to column-major
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                m_cachedInverse[c * 4 + r] = inv->GetElement(r, c);
            }
        }
        m_inverseDirtyFlag = false;
    }
    std::memcpy(out, m_cachedInverse, 16 * sizeof(double));
}

void TransformNode::multiplyBy(const double m[16])
{
    // Convert both to vtkMatrix4x4 (row-major), multiply, convert back
    vtkNew<vtkMatrix4x4> matA;
    vtkNew<vtkMatrix4x4> matB;

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            matA->SetElement(r, c, m_matrixData[c * 4 + r]);
            matB->SetElement(r, c, m[c * 4 + r]);
        }
    }

    vtkNew<vtkMatrix4x4> result;
    vtkMatrix4x4::Multiply4x4(matA, matB, result);

    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            m_matrixData[c * 4 + r] = result->GetElement(r, c);
        }
    }

    invalidateInverseCache();
    touchModified();
    emitEvent(NodeEventType::TransformChanged);
}

void TransformNode::inverse()
{
    double inv[16];
    getInverseMatrix(inv);
    std::memcpy(m_matrixData, inv, 16 * sizeof(double));
    invalidateInverseCache();
    touchModified();
    emitEvent(NodeEventType::TransformChanged);
}

bool TransformNode::isIdentity() const
{
    const double eps = 1e-12;
    for (int c = 0; c < 4; ++c) {
        for (int r = 0; r < 4; ++r) {
            double expected = (r == c) ? 1.0 : 0.0;
            if (std::fabs(m_matrixData[c * 4 + r] - expected) > eps) {
                return false;
            }
        }
    }
    return true;
}

void TransformNode::transformPoint(const double in[3], double out[3]) const
{
    // Column-major: result = M * [x, y, z, 1]^T
    for (int r = 0; r < 3; ++r) {
        out[r] = m_matrixData[0 * 4 + r] * in[0]
               + m_matrixData[1 * 4 + r] * in[1]
               + m_matrixData[2 * 4 + r] * in[2]
               + m_matrixData[3 * 4 + r]; // w=1
    }
    double w = m_matrixData[0 * 4 + 3] * in[0]
             + m_matrixData[1 * 4 + 3] * in[1]
             + m_matrixData[2 * 4 + 3] * in[2]
             + m_matrixData[3 * 4 + 3];
    if (std::fabs(w) > 1e-9 && std::fabs(w - 1.0) > 1e-15) {
        out[0] /= w;
        out[1] /= w;
        out[2] /= w;
    }
}

void TransformNode::transformVector(const double in[3], double out[3]) const
{
    // Vectors: no translation (w=0)
    for (int r = 0; r < 3; ++r) {
        out[r] = m_matrixData[0 * 4 + r] * in[0]
               + m_matrixData[1 * 4 + r] * in[1]
               + m_matrixData[2 * 4 + r] * in[2];
    }
}

void TransformNode::setTransformKind(const QString& kind)
{
    if (m_transformKind != kind) {
        m_transformKind = kind;
        touchModified();
    }
}

QString TransformNode::getTransformKind() const
{
    return m_transformKind;
}

void TransformNode::setSourceSpace(const QString& space)
{
    if (m_sourceSpaceName != space) {
        m_sourceSpaceName = space;
        touchModified();
    }
}

QString TransformNode::getSourceSpace() const
{
    return m_sourceSpaceName;
}

void TransformNode::setTargetSpace(const QString& space)
{
    if (m_targetSpaceName != space) {
        m_targetSpaceName = space;
        touchModified();
    }
}

QString TransformNode::getTargetSpace() const
{
    return m_targetSpaceName;
}

void TransformNode::setParentTransform(const QString& transformId)
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

QString TransformNode::getParentTransform() const
{
    return getFirstReference(NodeBase::parentTransformReferenceRole());
}

void TransformNode::setShowAxes(bool show)
{
    if (m_showAxesFlag != show) {
        m_showAxesFlag = show;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

bool TransformNode::isShowAxes() const
{
    return m_showAxesFlag;
}

void TransformNode::setAxesLength(double length)
{
    if (m_axesLengthValue != length) {
        m_axesLengthValue = length;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

double TransformNode::getAxesLength() const
{
    return m_axesLengthValue;
}

void TransformNode::setAxesColorX(const double rgba[4])
{
    for (int i = 0; i < 4; ++i) {
        m_axesColorX[i] = rgba[i];
    }
    emitEvent(NodeEventType::DisplayChanged);
}

void TransformNode::getAxesColorX(double out[4]) const
{
    for (int i = 0; i < 4; ++i) {
        out[i] = m_axesColorX[i];
    }
}

void TransformNode::setAxesColorY(const double rgba[4])
{
    for (int i = 0; i < 4; ++i) {
        m_axesColorY[i] = rgba[i];
    }
    emitEvent(NodeEventType::DisplayChanged);
}

void TransformNode::getAxesColorY(double out[4]) const
{
    for (int i = 0; i < 4; ++i) {
        out[i] = m_axesColorY[i];
    }
}

void TransformNode::setAxesColorZ(const double rgba[4])
{
    for (int i = 0; i < 4; ++i) {
        m_axesColorZ[i] = rgba[i];
    }
    emitEvent(NodeEventType::DisplayChanged);
}

void TransformNode::getAxesColorZ(double out[4]) const
{
    for (int i = 0; i < 4; ++i) {
        out[i] = m_axesColorZ[i];
    }
}

void TransformNode::invalidateInverseCache()
{
    m_inverseDirtyFlag = true;
}
