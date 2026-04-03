#pragma once

#include "NodeDisplayManager.h"

#include <vtkLineSource.h>
#include <vtkPolyDataMapper.h>
#include <vtkSmartPointer.h>
#include <vtkActor.h>

#include <array>
#include <QMap>
#include <QString>

class TransformNodeDisplayManager : public NodeDisplayManager
{
    Q_OBJECT

public:
    TransformNodeDisplayManager(SceneGraph* scene, const QString& windowId,
                                vtkRenderer* layer1, vtkRenderer* layer2, vtkRenderer* layer3,
                                QObject* parent = nullptr);
    ~TransformNodeDisplayManager() override;

    void onNodeAdded(const QString& nodeId) override;
    void onNodeRemoved(const QString& nodeId) override;
    void onNodeModified(const QString& nodeId, NodeEventType eventType) override;
    void reconcileWithScene() override;
    void clearAll() override;
    bool canHandleNode(NodeBase* node) const override;

private:
    struct TransformDisplayEntry {
        vtkSmartPointer<vtkLineSource> xAxisLineSource;
        vtkSmartPointer<vtkPolyDataMapper> xAxisMapper;
        vtkSmartPointer<vtkActor> xAxisActor;
        std::array<double, 3> xAxisOrigin = {0.0, 0.0, 0.0};
        std::array<double, 3> xAxisEndpoint = {0.0, 0.0, 0.0};
        std::array<double, 4> xAxisColor = {0.0, 0.0, 0.0, 1.0};
        bool xAxisInitialized = false;
        vtkSmartPointer<vtkLineSource> yAxisLineSource;
        vtkSmartPointer<vtkPolyDataMapper> yAxisMapper;
        vtkSmartPointer<vtkActor> yAxisActor;
        std::array<double, 3> yAxisOrigin = {0.0, 0.0, 0.0};
        std::array<double, 3> yAxisEndpoint = {0.0, 0.0, 0.0};
        std::array<double, 4> yAxisColor = {0.0, 0.0, 0.0, 1.0};
        bool yAxisInitialized = false;
        vtkSmartPointer<vtkLineSource> zAxisLineSource;
        vtkSmartPointer<vtkPolyDataMapper> zAxisMapper;
        vtkSmartPointer<vtkActor> zAxisActor;
        std::array<double, 3> zAxisOrigin = {0.0, 0.0, 0.0};
        std::array<double, 3> zAxisEndpoint = {0.0, 0.0, 0.0};
        std::array<double, 4> zAxisColor = {0.0, 0.0, 0.0, 1.0};
        bool zAxisInitialized = false;
        int currentLayer = 3;
    };

    void buildEntry(const QString& nodeId);
    void removeEntry(const QString& nodeId);
    void updateAxes(const QString& nodeId);
    void updateDisplay(const QString& nodeId);

    void computeAxisEndpoint(const double matrix[16], int axisIndex,
                             double length, double out[3]) const;

    QMap<QString, TransformDisplayEntry> m_entries;
};
