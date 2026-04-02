#include "VtkSceneWindow.h"

#include <QMetaObject>
#include <vtkCallbackCommand.h>
#include <vtkRenderWindow.h>

#include <cstring>

VtkSceneWindow::VtkSceneWindow(const QString& windowId, SceneGraph* sceneGraph,
                               QWidget* parent)
    : QWidget(parent)
    , m_windowId(windowId)
    , m_sceneGraph(sceneGraph)
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

        if (m_sceneGraph) {
        connect(m_sceneGraph, &SceneGraph::nodeAdded,
            this, &VtkSceneWindow::scheduleRender);
        connect(m_sceneGraph, &SceneGraph::nodeRemoved,
            this, &VtkSceneWindow::scheduleRender);
        connect(m_sceneGraph, &SceneGraph::nodeModified,
            this, [this](const QString&, NodeEventType) {
                scheduleRender();
            });
        connect(m_sceneGraph, &SceneGraph::batchModifyEnded,
            this, [this]() {
                reconcile();
                scheduleRender();
            });
        }

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
    m_interactionCallback = vtkSmartPointer<vtkCallbackCommand>::New();
    m_interactionCallback->SetClientData(this);
    m_interactionCallback->SetCallback([](vtkObject*, unsigned long, void* clientData, void*) {
        auto* self = static_cast<VtkSceneWindow*>(clientData);
        QMetaObject::invokeMethod(self, "onInteraction", Qt::QueuedConnection);
    });
    if (vtkRenderWindowInteractor* interactor = m_renderWindow->GetInteractor()) {
        m_interactionObserverTag = interactor->AddObserver(
            vtkCommand::InteractionEvent,
            m_interactionCallback);
    }
}

VtkSceneWindow::~VtkSceneWindow()
{
    m_isShuttingDown = true;
    m_renderQueued = false;

    if (m_cameraResetTimer) {
        m_cameraResetTimer->stop();
    }

    if (m_sceneGraph) {
        disconnect(m_sceneGraph, nullptr, this, nullptr);
    }

    detachInteractorObserver();

    for (auto* dm : m_displayManagers) {
        if (dm) {
            dm->clearAll();
        }
    }
    m_displayManagers.clear();

    teardownRenderWindow();
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
    if (m_isShuttingDown || !m_renderWindow) {
        return;
    }

    m_renderWindow->Render();
}

void VtkSceneWindow::reconcile()
{
    for (auto* dm : m_displayManagers) {
        dm->reconcileWithScene();
    }
}

void VtkSceneWindow::requestReconcile()
{
    if (m_isShuttingDown) {
        return;
    }

    reconcile();
    scheduleRender();
}

void VtkSceneWindow::onInteraction()
{
    if (m_isShuttingDown) {
        return;
    }

    m_cameraResetTimer->start();
}

void VtkSceneWindow::scheduleRender()
{
    if (m_isShuttingDown || m_renderQueued) {
        return;
    }

    m_renderQueued = true;
    QMetaObject::invokeMethod(this, "renderQueuedScene", Qt::QueuedConnection);
}

void VtkSceneWindow::renderQueuedScene()
{
    m_renderQueued = false;

    if (m_isShuttingDown) {
        return;
    }

    render();
}

void VtkSceneWindow::resetCameraToInitial()
{
    if (m_isShuttingDown || !m_camera || !m_renderWindow) {
        return;
    }

    m_camera->SetPosition(m_initialPosition);
    m_camera->SetFocalPoint(m_initialFocalPoint);
    m_camera->SetViewUp(m_initialViewUp);
    m_camera->SetParallelProjection(m_initialParallelProjection ? 1 : 0);
    m_camera->SetParallelScale(m_initialParallelScale);
    m_camera->SetViewAngle(m_initialViewAngle);
    m_camera->SetClippingRange(m_initialClippingRange);
    m_renderWindow->Render();
}

void VtkSceneWindow::detachInteractorObserver()
{
    if (!m_renderWindow || !m_interactionObserverTag) {
        return;
    }

    if (vtkRenderWindowInteractor* interactor = m_renderWindow->GetInteractor()) {
        interactor->RemoveObserver(m_interactionObserverTag);
    }

    m_interactionObserverTag = 0;
    m_interactionCallback = nullptr;
}

void VtkSceneWindow::teardownRenderWindow()
{
    if (!m_renderWindow) {
        return;
    }

    if (m_vtkWidget) {
        m_vtkWidget->setUpdatesEnabled(false);
        m_vtkWidget->setRenderWindow(static_cast<vtkGenericOpenGLRenderWindow*>(nullptr));
    }

    for (vtkSmartPointer<vtkRenderer>& renderer : m_renderers) {
        if (renderer) {
            m_renderWindow->RemoveRenderer(renderer);
            renderer = nullptr;
        }
    }

    m_renderWindow = nullptr;
}

void VtkSceneWindow::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    requestReconcile();
}
