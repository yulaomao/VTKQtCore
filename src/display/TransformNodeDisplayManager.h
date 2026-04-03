#pragma once

#include "NodeDisplayManager.h"

#include <vtkSmartPointer.h>
#include <vtkActor.h>
#include <vtkLineSource.h>
#include <vtkPolyDataMapper.h>

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
    struct AxisDisplayEntry {
        vtkSmartPointer<vtkActor> actor;
        vtkSmartPointer<vtkLineSource> lineSource;
        vtkSmartPointer<vtkPolyDataMapper> mapper;
        bool hasGeometry = false;
        bool hasColor = false;
        double cachedOrigin[3] = {0.0, 0.0, 0.0};
        double cachedEndpoint[3] = {0.0, 0.0, 0.0};
        double cachedColor[4] = {0.0, 0.0, 0.0, 0.0};
    };

    struct TransformDisplayEntry {
        AxisDisplayEntry xAxis;
        AxisDisplayEntry yAxis;
        AxisDisplayEntry zAxis;
        int currentLayer = 3;
        bool visible = false;
        bool geometryDirty = true;
    };

    void buildEntry(const QString& nodeId);
    void removeEntry(const QString& nodeId);
    void updateAxes(const QString& nodeId);
    void updateDisplay(const QString& nodeId);
    void updateAxisGeometry(AxisDisplayEntry& axis, const double origin[3], const double endpoint[3]);
    void updateAxisColor(AxisDisplayEntry& axis, const double color[4]);

    void computeAxisEndpoint(const double matrix[16], int axisIndex,
                             double length, double out[3]) const;

    QMap<QString, TransformDisplayEntry> m_entries;
};
