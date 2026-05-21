#pragma once

#include "NodeBase.h"

#include <vtkSmartPointer.h>

#include <QString>
#include <array>

class vtkPolyData;

class PlaneNode : public NodeBase
{
    Q_OBJECT

public:
    explicit PlaneNode(QObject* parent = nullptr);
    ~PlaneNode() override = default;

    void setParentTransform(const QString& transformId);
    QString getParentTransform() const;

    void setPlaneSize(double width, double height);
    double getPlaneWidth() const;
    double getPlaneHeight() const;

    void setCenter(double x, double y, double z);
    std::array<double, 3> getCenter() const;

    void setNormal(double x, double y, double z);
    std::array<double, 3> getNormal() const;

    void setPlaneColor(const double rgba[4]);
    void getPlaneColor(double out[4]) const;

    void setPlaneOpacity(double opacity);
    double getPlaneOpacity() const;

    void setBorderColor(const double rgba[4]);
    void getBorderColor(double out[4]) const;

    void setBorderOpacity(double opacity);
    double getBorderOpacity() const;

    void setBorderWidth(double width);
    double getBorderWidth() const;

    vtkSmartPointer<vtkPolyData> getSurfacePolyData() const;
    vtkSmartPointer<vtkPolyData> getBorderPolyData() const;

private:
    void rebuildGeometry();

    double m_width = 36.0;
    double m_height = 24.0;
    std::array<double, 3> m_center = {0.0, 0.0, 0.0};
    std::array<double, 3> m_normal = {0.0, 0.0, 1.0};
    double m_planeColor[4] = {0.28, 0.68, 0.94, 0.35};
    double m_planeOpacity = 0.35;
    double m_borderColor[4] = {0.05, 0.12, 0.2, 1.0};
    double m_borderOpacity = 1.0;
    double m_borderWidth = 2.0;
    vtkSmartPointer<vtkPolyData> m_surfacePolyData;
    vtkSmartPointer<vtkPolyData> m_borderPolyData;
};