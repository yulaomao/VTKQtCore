#include "ModelNode.h"

#include "../SceneGraph.h"

#include <QtGlobal>

#include <vtkNew.h>
#include <vtkTriangle.h>

ModelNode::ModelNode(QObject* parent)
    : NodeBase(QStringLiteral("ModelNode"), parent)
{
    DisplayTarget dt;
    dt.visible = true;
    dt.layer = 1;
    setDefaultDisplayTarget(dt);
}

void ModelNode::setPolyData(vtkSmartPointer<vtkPolyData> polyData)
{
    m_polyDataPayload = polyData;
    updateCachedBounds();
    touchModified();
}

vtkSmartPointer<vtkPolyData> ModelNode::getPolyData() const
{
    return m_polyDataPayload;
}

void ModelNode::clearPolyData()
{
    if (m_polyDataPayload) {
        m_polyDataPayload = nullptr;
        for (int i = 0; i < 6; ++i) {
            m_cachedBounds[i] = 0.0;
        }
        touchModified();
    }
}

void ModelNode::setMeshData(const QVector<std::array<double, 3>>& vertices,
                            const QVector<std::array<int, 3>>& indices)
{
    vtkNew<vtkPolyData> polyData;
    vtkNew<vtkPoints> points;
    vtkNew<vtkCellArray> triangles;

    points->SetNumberOfPoints(vertices.size());
    for (int i = 0; i < vertices.size(); ++i) {
        points->SetPoint(i, vertices[i][0], vertices[i][1], vertices[i][2]);
    }

    for (const auto& tri : indices) {
        vtkNew<vtkTriangle> triangle;
        triangle->GetPointIds()->SetId(0, tri[0]);
        triangle->GetPointIds()->SetId(1, tri[1]);
        triangle->GetPointIds()->SetId(2, tri[2]);
        triangles->InsertNextCell(triangle);
    }

    polyData->SetPoints(points);
    polyData->SetPolys(triangles);

    m_polyDataPayload = polyData;
    updateCachedBounds();
    touchModified();
}

QVector<std::array<double, 3>> ModelNode::getVertices() const
{
    QVector<std::array<double, 3>> result;
    if (!m_polyDataPayload || !m_polyDataPayload->GetPoints()) {
        return result;
    }
    vtkPoints* points = m_polyDataPayload->GetPoints();
    vtkIdType numPoints = points->GetNumberOfPoints();
    result.reserve(static_cast<int>(numPoints));
    for (vtkIdType i = 0; i < numPoints; ++i) {
        double p[3];
        points->GetPoint(i, p);
        result.append({p[0], p[1], p[2]});
    }
    return result;
}

QVector<std::array<int, 3>> ModelNode::getIndices() const
{
    QVector<std::array<int, 3>> result;
    if (!m_polyDataPayload || !m_polyDataPayload->GetPolys()) {
        return result;
    }
    vtkCellArray* polys = m_polyDataPayload->GetPolys();
    polys->InitTraversal();
    vtkNew<vtkIdList> idList;
    while (polys->GetNextCell(idList)) {
        if (idList->GetNumberOfIds() == 3) {
            result.append({static_cast<int>(idList->GetId(0)),
                           static_cast<int>(idList->GetId(1)),
                           static_cast<int>(idList->GetId(2))});
        }
    }
    return result;
}

void ModelNode::getBoundingBox(double out[6]) const
{
    for (int i = 0; i < 6; ++i) {
        out[i] = m_cachedBounds[i];
    }
}

void ModelNode::setVisibility(bool visible)
{
    if (m_visibilityFlag != visible) {
        m_visibilityFlag = visible;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

bool ModelNode::isVisible() const
{
    return m_visibilityFlag;
}

void ModelNode::setOpacity(double opacity)
{
    if (m_opacityValue != opacity) {
        m_opacityValue = opacity;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

double ModelNode::getOpacity() const
{
    return m_opacityValue;
}

void ModelNode::setColor(const double rgba[4])
{
    for (int i = 0; i < 4; ++i) {
        m_colorValue[i] = rgba[i];
    }
    emitEvent(NodeEventType::DisplayChanged);
}

void ModelNode::getColor(double out[4]) const
{
    for (int i = 0; i < 4; ++i) {
        out[i] = m_colorValue[i];
    }
}

void ModelNode::setParentTransform(const QString& transformId)
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

QString ModelNode::getParentTransform() const
{
    return getFirstReference(NodeBase::parentTransformReferenceRole());
}

void ModelNode::setRenderMode(const QString& mode)
{
    if (m_renderMode != mode) {
        m_renderMode = mode;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

QString ModelNode::getRenderMode() const
{
    return m_renderMode;
}

void ModelNode::setShowEdges(bool show)
{
    if (m_showEdgesFlag != show) {
        m_showEdgesFlag = show;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

bool ModelNode::isShowEdges() const
{
    return m_showEdgesFlag;
}

void ModelNode::setEdgeColor(const double rgba[4])
{
    for (int i = 0; i < 4; ++i) {
        m_edgeColorRGBA[i] = rgba[i];
    }
    emitEvent(NodeEventType::DisplayChanged);
}

void ModelNode::getEdgeColor(double out[4]) const
{
    for (int i = 0; i < 4; ++i) {
        out[i] = m_edgeColorRGBA[i];
    }
}

void ModelNode::setEdgeWidth(double width)
{
    if (m_edgeWidthValue != width) {
        m_edgeWidthValue = width;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

double ModelNode::getEdgeWidth() const
{
    return m_edgeWidthValue;
}

void ModelNode::setBackfaceCulling(bool enable)
{
    if (m_backfaceCullingFlag != enable) {
        m_backfaceCullingFlag = enable;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

bool ModelNode::isBackfaceCulling() const
{
    return m_backfaceCullingFlag;
}

void ModelNode::setMaterialAmbient(double ambient)
{
    const double normalized = qBound(0.0, ambient, 1.0);
    if (m_materialAmbientValue != normalized) {
        m_materialAmbientValue = normalized;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

double ModelNode::getMaterialAmbient() const
{
    return m_materialAmbientValue;
}

void ModelNode::setMaterialDiffuse(double diffuse)
{
    const double normalized = qBound(0.0, diffuse, 1.0);
    if (m_materialDiffuseValue != normalized) {
        m_materialDiffuseValue = normalized;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

double ModelNode::getMaterialDiffuse() const
{
    return m_materialDiffuseValue;
}

void ModelNode::setMaterialSpecular(double specular)
{
    const double normalized = qBound(0.0, specular, 1.0);
    if (m_materialSpecularValue != normalized) {
        m_materialSpecularValue = normalized;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

double ModelNode::getMaterialSpecular() const
{
    return m_materialSpecularValue;
}

void ModelNode::setMaterialSpecularPower(double power)
{
    const double normalized = qMax(0.0, power);
    if (m_materialSpecularPowerValue != normalized) {
        m_materialSpecularPowerValue = normalized;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

double ModelNode::getMaterialSpecularPower() const
{
    return m_materialSpecularPowerValue;
}

void ModelNode::setMaterialRoughness(double roughness)
{
    const double normalized = qBound(0.0, roughness, 1.0);
    if (m_materialRoughnessValue != normalized) {
        m_materialRoughnessValue = normalized;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

double ModelNode::getMaterialRoughness() const
{
    return m_materialRoughnessValue;
}

void ModelNode::setUseScalarColor(bool use)
{
    if (m_useScalarColorFlag != use) {
        m_useScalarColorFlag = use;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

bool ModelNode::isUseScalarColor() const
{
    return m_useScalarColorFlag;
}

void ModelNode::setScalarColorMap(const QString& name)
{
    if (m_scalarColorMapName != name) {
        m_scalarColorMapName = name;
        emitEvent(NodeEventType::DisplayChanged);
    }
}

QString ModelNode::getScalarColorMap() const
{
    return m_scalarColorMapName;
}

void ModelNode::setModelRole(const QString& role)
{
    if (m_modelRole != role) {
        m_modelRole = role;
        touchModified();
    }
}

QString ModelNode::getModelRole() const
{
    return m_modelRole;
}

void ModelNode::updateCachedBounds()
{
    if (m_polyDataPayload) {
        m_polyDataPayload->GetBounds(m_cachedBounds);
    } else {
        for (int i = 0; i < 6; ++i) {
            m_cachedBounds[i] = 0.0;
        }
    }
}
