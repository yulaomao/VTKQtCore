#pragma once

#include "NodeDisplayManager.h"

#include <vtkActor.h>
#include <vtkMatrix4x4.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>

#include <QMap>
#include <QString>

class PlaneNodeDisplayManager : public NodeDisplayManager
{
    Q_OBJECT

public:
    PlaneNodeDisplayManager(SceneGraph* scene, const QString& windowId,
                            vtkRenderer* layer1, vtkRenderer* layer2, vtkRenderer* layer3,
                            QObject* parent = nullptr);
    ~PlaneNodeDisplayManager() override;

    void onNodeAdded(const QString& nodeId) override;
    void onNodeRemoved(const QString& nodeId) override;
    void onNodeModified(const QString& nodeId, NodeEventType eventType) override;
    void reconcileWithScene() override;
    void clearAll() override;
    bool canHandleNode(NodeBase* node) const override;

private:
    struct PlaneDisplayEntry {
        vtkSmartPointer<vtkActor> fillActor;
        vtkSmartPointer<vtkActor> borderActor;
        vtkSmartPointer<vtkPolyDataMapper> fillMapper;
        vtkSmartPointer<vtkPolyDataMapper> borderMapper;
        vtkSmartPointer<vtkPolyData> surfacePolyData;
        vtkSmartPointer<vtkPolyData> borderPolyData;
        vtkSmartPointer<vtkTransform> transform;
        vtkSmartPointer<vtkMatrix4x4> transformMatrix;
        int currentLayer = 1;
        bool fillVisible = false;
        bool borderVisible = false;
        bool hasFillColor = false;
        bool hasFillOpacity = false;
        bool hasBorderColor = false;
        bool hasBorderOpacity = false;
        bool hasBorderWidth = false;
        bool hasWorldTransform = false;
        double cachedFillColor[4] = {0.0, 0.0, 0.0, 0.0};
        double cachedFillOpacity = 1.0;
        double cachedBorderColor[4] = {0.0, 0.0, 0.0, 0.0};
        double cachedBorderOpacity = 1.0;
        double cachedBorderWidth = 1.0;
        double cachedWorldMatrix[16] = {0.0};
    };

    void buildEntry(const QString& nodeId);
    void removeEntry(const QString& nodeId);
    void updateContent(const QString& nodeId);
    void updateDisplay(const QString& nodeId);
    void updateTransform(const QString& nodeId);

    QMap<QString, PlaneDisplayEntry> m_entries;
};