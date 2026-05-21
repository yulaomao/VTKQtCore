#include "PlaneNode.h"

#include "../SceneGraph.h"

#include <vtkCellArray.h>
#include <vtkLine.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkQuad.h>

#include <algorithm>
#include <cmath>

namespace {

constexpr double kTolerance = 1e-9;

bool areClose(double lhs, double rhs)
{
    return std::abs(lhs - rhs) <= kTolerance;
}

bool areVec3Equal(const std::array<double, 3>& lhs, const std::array<double, 3>& rhs)
{
    return areClose(lhs[0], rhs[0]) && areClose(lhs[1], rhs[1]) && areClose(lhs[2], rhs[2]);
}

bool areColorEqual(const double* lhs, const double* rhs)
{
    return areClose(lhs[0], rhs[0]) && areClose(lhs[1], rhs[1]) && areClose(lhs[2], rhs[2]) && areClose(lhs[3], rhs[3]);
}

std::array<double, 3> normalizeVector(const std::array<double, 3>& vector)
{
    const double length = std::sqrt(vector[0] * vector[0] +
                                    vector[1] * vector[1] +
                                    vector[2] * vector[2]);
    if (length <= kTolerance) {
        return {0.0, 0.0, 1.0};
    }

    return {vector[0] / length, vector[1] / length, vector[2] / length};
}

std::array<double, 3> crossProduct(const std::array<double, 3>& lhs,
                                   const std::array<double, 3>& rhs)
{
    return {
        lhs[1] * rhs[2] - lhs[2] * rhs[1],
        lhs[2] * rhs[0] - lhs[0] * rhs[2],
        lhs[0] * rhs[1] - lhs[1] * rhs[0]
    };
}

}

PlaneNode::PlaneNode(QObject* parent)
    : NodeBase(QStringLiteral("PlaneNode"), parent)
{
    DisplayTarget dt;
    dt.visible = true;
    dt.layer = 1;
    setDefaultDisplayTarget(dt);
    rebuildGeometry();
}

void PlaneNode::setParentTransform(const QString& transformId)
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

QString PlaneNode::getParentTransform() const
{
    return getFirstReference(NodeBase::parentTransformReferenceRole());
}

void PlaneNode::setPlaneSize(double width, double height)
{
    const double safeWidth = std::max(0.01, width);
    const double safeHeight = std::max(0.01, height);
    if (areClose(m_width, safeWidth) && areClose(m_height, safeHeight)) {
        return;
    }

    m_width = safeWidth;
    m_height = safeHeight;
    rebuildGeometry();
    touchModified();
}

double PlaneNode::getPlaneWidth() const
{
    return m_width;
}

double PlaneNode::getPlaneHeight() const
{
    return m_height;
}

void PlaneNode::setCenter(double x, double y, double z)
{
    const std::array<double, 3> center = {x, y, z};
    if (areVec3Equal(m_center, center)) {
        return;
    }

    m_center = center;
    rebuildGeometry();
    touchModified();
}

std::array<double, 3> PlaneNode::getCenter() const
{
    return m_center;
}

void PlaneNode::setNormal(double x, double y, double z)
{
    const std::array<double, 3> normal = normalizeVector({x, y, z});
    if (areVec3Equal(m_normal, normal)) {
        return;
    }

    m_normal = normal;
    rebuildGeometry();
    touchModified();
}

std::array<double, 3> PlaneNode::getNormal() const
{
    return m_normal;
}

void PlaneNode::setPlaneColor(const double rgba[4])
{
    if (!rgba || areColorEqual(m_planeColor, rgba)) {
        return;
    }

    for (int index = 0; index < 4; ++index) {
        m_planeColor[index] = rgba[index];
    }
    emitEvent(NodeEventType::DisplayChanged);
}

void PlaneNode::getPlaneColor(double out[4]) const
{
    if (!out) {
        return;
    }

    for (int index = 0; index < 4; ++index) {
        out[index] = m_planeColor[index];
    }
}

void PlaneNode::setPlaneOpacity(double opacity)
{
    const double safeOpacity = std::clamp(opacity, 0.0, 1.0);
    if (areClose(m_planeOpacity, safeOpacity)) {
        return;
    }

    m_planeOpacity = safeOpacity;
    emitEvent(NodeEventType::DisplayChanged);
}

double PlaneNode::getPlaneOpacity() const
{
    return m_planeOpacity;
}

void PlaneNode::setBorderColor(const double rgba[4])
{
    if (!rgba || areColorEqual(m_borderColor, rgba)) {
        return;
    }

    for (int index = 0; index < 4; ++index) {
        m_borderColor[index] = rgba[index];
    }
    emitEvent(NodeEventType::DisplayChanged);
}

void PlaneNode::getBorderColor(double out[4]) const
{
    if (!out) {
        return;
    }

    for (int index = 0; index < 4; ++index) {
        out[index] = m_borderColor[index];
    }
}

void PlaneNode::setBorderOpacity(double opacity)
{
    const double safeOpacity = std::clamp(opacity, 0.0, 1.0);
    if (areClose(m_borderOpacity, safeOpacity)) {
        return;
    }

    m_borderOpacity = safeOpacity;
    emitEvent(NodeEventType::DisplayChanged);
}

double PlaneNode::getBorderOpacity() const
{
    return m_borderOpacity;
}

void PlaneNode::setBorderWidth(double width)
{
    const double safeWidth = std::max(0.0, width);
    if (areClose(m_borderWidth, safeWidth)) {
        return;
    }

    m_borderWidth = safeWidth;
    emitEvent(NodeEventType::DisplayChanged);
}

double PlaneNode::getBorderWidth() const
{
    return m_borderWidth;
}

vtkSmartPointer<vtkPolyData> PlaneNode::getSurfacePolyData() const
{
    return m_surfacePolyData;
}

vtkSmartPointer<vtkPolyData> PlaneNode::getBorderPolyData() const
{
    return m_borderPolyData;
}

void PlaneNode::rebuildGeometry()
{
    const std::array<double, 3> normal = normalizeVector(m_normal);
    const std::array<double, 3> reference = std::abs(normal[2]) < 0.95
        ? std::array<double, 3>{0.0, 0.0, 1.0}
        : std::array<double, 3>{0.0, 1.0, 0.0};
    const std::array<double, 3> axisU = normalizeVector(crossProduct(reference, normal));
    const std::array<double, 3> axisV = normalizeVector(crossProduct(normal, axisU));

    const double halfWidth = m_width * 0.5;
    const double halfHeight = m_height * 0.5;
    const std::array<std::array<double, 3>, 4> corners = {{
        {
            m_center[0] - axisU[0] * halfWidth - axisV[0] * halfHeight,
            m_center[1] - axisU[1] * halfWidth - axisV[1] * halfHeight,
            m_center[2] - axisU[2] * halfWidth - axisV[2] * halfHeight
        },
        {
            m_center[0] + axisU[0] * halfWidth - axisV[0] * halfHeight,
            m_center[1] + axisU[1] * halfWidth - axisV[1] * halfHeight,
            m_center[2] + axisU[2] * halfWidth - axisV[2] * halfHeight
        },
        {
            m_center[0] + axisU[0] * halfWidth + axisV[0] * halfHeight,
            m_center[1] + axisU[1] * halfWidth + axisV[1] * halfHeight,
            m_center[2] + axisU[2] * halfWidth + axisV[2] * halfHeight
        },
        {
            m_center[0] - axisU[0] * halfWidth + axisV[0] * halfHeight,
            m_center[1] - axisU[1] * halfWidth + axisV[1] * halfHeight,
            m_center[2] - axisU[2] * halfWidth + axisV[2] * halfHeight
        }
    }};

    auto surfacePoints = vtkSmartPointer<vtkPoints>::New();
    for (const auto& corner : corners) {
        surfacePoints->InsertNextPoint(corner[0], corner[1], corner[2]);
    }

    auto quad = vtkSmartPointer<vtkQuad>::New();
    for (vtkIdType index = 0; index < 4; ++index) {
        quad->GetPointIds()->SetId(index, index);
    }

    auto surfaceCells = vtkSmartPointer<vtkCellArray>::New();
    surfaceCells->InsertNextCell(quad);

    m_surfacePolyData = vtkSmartPointer<vtkPolyData>::New();
    m_surfacePolyData->SetPoints(surfacePoints);
    m_surfacePolyData->SetPolys(surfaceCells);

    auto borderPoints = vtkSmartPointer<vtkPoints>::New();
    for (const auto& corner : corners) {
        borderPoints->InsertNextPoint(corner[0], corner[1], corner[2]);
    }

    auto borderLines = vtkSmartPointer<vtkCellArray>::New();
    for (vtkIdType index = 0; index < 4; ++index) {
        auto line = vtkSmartPointer<vtkLine>::New();
        line->GetPointIds()->SetId(0, index);
        line->GetPointIds()->SetId(1, (index + 1) % 4);
        borderLines->InsertNextCell(line);
    }

    m_borderPolyData = vtkSmartPointer<vtkPolyData>::New();
    m_borderPolyData->SetPoints(borderPoints);
    m_borderPolyData->SetLines(borderLines);
}