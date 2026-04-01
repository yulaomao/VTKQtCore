#include "VtkSceneWindow.h"

#include <vtkCallbackCommand.h>
#include <vtkRenderWindow.h>

#include <cstring>

VtkSceneWindow::VtkSceneWindow(const QString& windowId, SceneGraph* sceneGraph,
                               QWidget* parent)
    : QWidget(parent)
    , m_windowId(windowId)
    , m_vtkWidget(new QVTKOpenGLNativeWidget(this))
    , m_renderWindow(vtkSmartPointer<vtkGenericOpenGLRenderWindow>::New())
    , m_camera(vtkSmartPointer<vtkCamera>::New())
    , m_cameraResetTimer(new QTimer(this))
    , m_initialParallelProjection(false)
    , m_initialParallelScale(1.0)
    , m_initialViewAngle(30.0)
{
    std::memset(m_initialPosition, 0, sizeof(m_initialPosition));
    std::memset(m_initialFocalPoint, 0, sizeof(m_initialFocalPoint));
    std::memset(m_initialViewUp, 0, sizeof(m_initialViewUp));
    std::memset(m_initialClippingRange, 0, sizeof(m_initialClippingRange));

    m_vtkWidget->setRenderWindow(m_renderWindow);

    // Layer 0 (base)
    m_renderers[0] = vtkSmartPointer<vtkRenderer>::New();
    m_renderers[0]->SetLayer(0);
    m_renderers[0]->SetBackground(0.2, 0.3, 0.4);
    m_renderers[0]->SetActiveCamera(m_camera);

    // Layer 1 (mid)
    m_renderers[1] = vtkSmartPointer<vtkRenderer>::New();
    m_renderers[1]->SetLayer(1);
    m_renderers[1]->SetBackground(0.0, 0.0, 0.0);
    m_renderers[1]->SetBackgroundAlpha(0.0);
    m_renderers[1]->SetActiveCamera(m_camera);

    // Layer 2 (top)
    m_renderers[2] = vtkSmartPointer<vtkRenderer>::New();
    m_renderers[2]->SetLayer(2);
    m_renderers[2]->SetBackground(0.0, 0.0, 0.0);
    m_renderers[2]->SetBackgroundAlpha(0.0);
    m_renderers[2]->SetActiveCamera(m_camera);

    m_renderWindow->SetNumberOfLayers(3);
    m_renderWindow->AddRenderer(m_renderers[0]);
    m_renderWindow->AddRenderer(m_renderers[1]);
    m_renderWindow->AddRenderer(m_renderers[2]);

    // Create display managers
    auto* pointDM = new PointNodeDisplayManager(
        sceneGraph, windowId,
        m_renderers[0], m_renderers[1], m_renderers[2], this);
    auto* lineDM = new LineNodeDisplayManager(
        sceneGraph, windowId,
        m_renderers[0], m_renderers[1], m_renderers[2], this);
    auto* modelDM = new ModelNodeDisplayManager(
        sceneGraph, windowId,
        m_renderers[0], m_renderers[1], m_renderers[2], this);
    auto* transformDM = new TransformNodeDisplayManager(
        sceneGraph, windowId,
        m_renderers[0], m_renderers[1], m_renderers[2], this);

    m_displayManagers.append(pointDM);
    m_displayManagers.append(lineDM);
    m_displayManagers.append(modelDM);
    m_displayManagers.append(transformDM);

    // Layout
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_vtkWidget);
    setLayout(layout);

    // Camera reset timer
    m_cameraResetTimer->setSingleShot(true);
    m_cameraResetTimer->setInterval(3000);
    connect(m_cameraResetTimer, &QTimer::timeout,
            this, &VtkSceneWindow::resetCameraToInitial);

    // Connect interactor's InteractionEvent to restart the timer
    vtkSmartPointer<vtkCallbackCommand> callback =
        vtkSmartPointer<vtkCallbackCommand>::New();
    callback->SetClientData(this);
    callback->SetCallback([](vtkObject*, unsigned long, void* clientData, void*) {
        auto* self = static_cast<VtkSceneWindow*>(clientData);
        QMetaObject::invokeMethod(self, "onInteraction", Qt::QueuedConnection);
    });
    m_renderWindow->GetInteractor()->AddObserver(
        vtkCommand::InteractionEvent, callback);
}

QString VtkSceneWindow::getWindowId() const
{
    return m_windowId;
}

vtkRenderWindow* VtkSceneWindow::getRenderWindow() const
{
    return m_renderWindow;
}

vtkRenderer* VtkSceneWindow::getRenderer(int layer) const
{
    if (layer >= 1 && layer <= 3) {
        return m_renderers[layer - 1];
    }
    return nullptr;
}

void VtkSceneWindow::setInitialCameraParams(const double position[3],
                                             const double focalPoint[3],
                                             const double viewUp[3],
                                             bool parallelProjection,
                                             double parallelScale,
                                             double viewAngle,
                                             const double clippingRange[2])
{
    std::memcpy(m_initialPosition, position, 3 * sizeof(double));
    std::memcpy(m_initialFocalPoint, focalPoint, 3 * sizeof(double));
    std::memcpy(m_initialViewUp, viewUp, 3 * sizeof(double));
    m_initialParallelProjection = parallelProjection;
    m_initialParallelScale = parallelScale;
    m_initialViewAngle = viewAngle;
    std::memcpy(m_initialClippingRange, clippingRange, 2 * sizeof(double));

    // Apply immediately
    resetCameraToInitial();
}

void VtkSceneWindow::render()
{
    m_renderWindow->Render();
}

void VtkSceneWindow::reconcile()
{
    for (auto* dm : m_displayManagers) {
        dm->reconcileWithScene();
    }
}

void VtkSceneWindow::onInteraction()
{
    m_cameraResetTimer->start();
}

void VtkSceneWindow::resetCameraToInitial()
{
    m_camera->SetPosition(m_initialPosition);
    m_camera->SetFocalPoint(m_initialFocalPoint);
    m_camera->SetViewUp(m_initialViewUp);
    m_camera->SetParallelProjection(m_initialParallelProjection ? 1 : 0);
    m_camera->SetParallelScale(m_initialParallelScale);
    m_camera->SetViewAngle(m_initialViewAngle);
    m_camera->SetClippingRange(m_initialClippingRange);
    m_renderWindow->Render();
}
