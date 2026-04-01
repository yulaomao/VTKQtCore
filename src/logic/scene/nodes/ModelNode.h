#pragma once

#include "NodeBase.h"

#include <vtkSmartPointer.h>
#include <vtkPolyData.h>
#include <vtkPoints.h>
#include <vtkCellArray.h>

#include <QVector>
#include <array>

class ModelNode : public NodeBase
{
    Q_OBJECT

public:
    explicit ModelNode(QObject* parent = nullptr);
    ~ModelNode() override = default;

    void setPolyData(vtkSmartPointer<vtkPolyData> polyData);
    vtkSmartPointer<vtkPolyData> getPolyData() const;
    void clearPolyData();

    void setMeshData(const QVector<std::array<double, 3>>& vertices,
                     const QVector<std::array<int, 3>>& indices);
    QVector<std::array<double, 3>> getVertices() const;
    QVector<std::array<int, 3>> getIndices() const;

    void getBoundingBox(double out[6]) const;

    void setVisibility(bool visible);
    bool isVisible() const;

    void setOpacity(double opacity);
    double getOpacity() const;

    void setColor(const double rgba[4]);
    void getColor(double out[4]) const;

    void setParentTransform(const QString& transformId);
    QString getParentTransform() const;

    void setRenderMode(const QString& mode);
    QString getRenderMode() const;

    void setShowEdges(bool show);
    bool isShowEdges() const;

    void setEdgeColor(const double rgba[4]);
    void getEdgeColor(double out[4]) const;

    void setEdgeWidth(double width);
    double getEdgeWidth() const;

    void setBackfaceCulling(bool enable);
    bool isBackfaceCulling() const;

    void setUseScalarColor(bool use);
    bool isUseScalarColor() const;

    void setScalarColorMap(const QString& name);
    QString getScalarColorMap() const;

    void setModelRole(const QString& role);
    QString getModelRole() const;

private:
    void updateCachedBounds();

    vtkSmartPointer<vtkPolyData> m_polyDataPayload;
    bool m_visibilityFlag = true;
    double m_opacityValue = 1.0;
    double m_colorValue[4] = {0.8, 0.8, 0.8, 1.0};
    QString m_modelRole;
    double m_cachedBounds[6] = {0, 0, 0, 0, 0, 0};
    QString m_parentTransformId;
    QString m_renderMode = QStringLiteral("surface");
    bool m_showEdgesFlag = false;
    double m_edgeColorRGBA[4] = {0.0, 0.0, 0.0, 1.0};
    double m_edgeWidthValue = 1.0;
    bool m_backfaceCullingFlag = false;
    bool m_useScalarColorFlag = false;
    QString m_scalarColorMapName;
};
