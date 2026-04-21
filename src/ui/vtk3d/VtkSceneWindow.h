#pragma once

#include <QWidget>
#include <QVTKOpenGLNativeWidget.h>
#include <vtkGenericOpenGLRenderWindow.h>
#include <vtkRenderer.h>
#include <vtkCamera.h>
#include <vtkCallbackCommand.h>
#include <vtkSmartPointer.h>
#include <vtkRenderWindowInteractor.h>
#include <QTimer>
#include <QVBoxLayout>
#include <QVector>
#include <QString>
#include <QPointF>
#include <QShowEvent>
#include <QTouchEvent>

#include "display/NodeDisplayManager.h"
#include "display/BillboardArrowNodeDisplayManager.h"
#include "display/BillboardLineNodeDisplayManager.h"
#include "display/PointNodeDisplayManager.h"
#include "display/LineNodeDisplayManager.h"
#include "display/ModelNodeDisplayManager.h"
#include "display/TransformNodeDisplayManager.h"
#include "logic/scene/SceneGraph.h"

class VtkSceneWindow : public QWidget
{
    Q_OBJECT

public:
    VtkSceneWindow(const QString& windowId, SceneGraph* sceneGraph,
                   QWidget* parent = nullptr);
    ~VtkSceneWindow() override;

    QString getWindowId() const;
    vtkRenderWindow* getRenderWindow() const;
    vtkRenderer* getRenderer(int layer) const;

    void setInitialCameraParams(const double position[3],
                                const double focalPoint[3],
                                const double viewUp[3],
                                bool parallelProjection,
                                double parallelScale,
                                double viewAngle,
                                const double clippingRange[2]);
    void render();
    void reconcile();

public slots:
    void requestReconcile();

private slots:
    void onInteraction();
    void resetCameraToInitial();
    void scheduleRender();
    void renderQueuedScene();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    bool handleTouchBegin(QTouchEvent* event);
    bool handleTouchUpdate(QTouchEvent* event);
    bool handleTouchEnd(QTouchEvent* event);
    void rotateCamera(double deltaX, double deltaY);
    void panCamera(double deltaX, double deltaY);
    double calculateDistance(const QPointF& point1, const QPointF& point2) const;
    void renderAfterTouch();
    void resetTouchState();
    bool shouldBlockMouseEvent(QEvent::Type eventType) const;
    void detachInteractorObserver();
    void teardownRenderWindow();

    QString m_windowId;
    SceneGraph* m_sceneGraph = nullptr;
    QVTKOpenGLNativeWidget* m_vtkWidget;
    vtkSmartPointer<vtkGenericOpenGLRenderWindow> m_renderWindow;
    vtkSmartPointer<vtkRenderer> m_renderers[3];
    vtkSmartPointer<vtkCamera> m_camera;
    vtkSmartPointer<vtkCallbackCommand> m_interactionCallback;
    QVector<NodeDisplayManager*> m_displayManagers;
    QTimer* m_cameraResetTimer;

    double m_initialPosition[3];
    double m_initialFocalPoint[3];
    double m_initialViewUp[3];
    bool m_initialParallelProjection;
    double m_initialParallelScale;
    double m_initialViewAngle;
    double m_initialClippingRange[2];
    unsigned long m_interactionObserverTag = 0;
    bool m_isShuttingDown = false;
    bool m_renderQueued = false;
    bool m_touchSequenceActive = false;
    bool m_isPinching = false;
    bool m_isRotating = false;
    double m_initialPinchDistance = 0.0;
    double m_currentCameraDistance = 0.0;
    double m_currentParallelScale = 1.0;
    QPointF m_lastPinchCenter;
    QPointF m_lastRotatePos;
};
