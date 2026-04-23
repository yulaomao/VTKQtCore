#include "VtkSceneWindow.h"

#include <QMetaObject>
#include <QEvent>
#include <vtkMath.h>
#include <vtkCallbackCommand.h>
#include <vtkRenderWindow.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

QVector<QPointF> extractTouchPositions(const QTouchEvent* event)
{
    QVector<QPointF> positions;

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    const auto points = event->points();
    positions.reserve(static_cast<int>(points.size()));
    for (const auto& point : points) {
        positions.push_back(point.position());
    }
#else
    const auto points = event->touchPoints();
    positions.reserve(points.size());
    for (const auto& point : points) {
        positions.push_back(point.pos());
    }
#endif

    return positions;
}

QPointF pinchCenter(const QPointF& point1, const QPointF& point2)
{
    return QPointF((point1.x() + point2.x()) * 0.5,
                   (point1.y() + point2.y()) * 0.5);
}

}

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

    m_vtkWidget->setAttribute(Qt::WA_AcceptTouchEvents, true);
    m_vtkWidget->installEventFilter(this);
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
    auto* billboardLineDM = new BillboardLineNodeDisplayManager(
        sceneGraph, windowId,
        m_renderers[0], m_renderers[1], m_renderers[2], this);
    auto* billboardArrowDM = new BillboardArrowNodeDisplayManager(
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
    m_displayManagers.append(billboardLineDM);
    m_displayManagers.append(billboardArrowDM);
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

    if (m_vtkWidget) {
        m_vtkWidget->removeEventFilter(this);
    }

    detachInteractorObserver();

    qDeleteAll(m_displayManagers);
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

    restartCameraResetTimer();
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

bool VtkSceneWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_vtkWidget && event != nullptr) {
        switch (event->type()) {
        case QEvent::TouchBegin:
            return handleTouchBegin(static_cast<QTouchEvent*>(event));
        case QEvent::TouchUpdate:
            return handleTouchUpdate(static_cast<QTouchEvent*>(event));
        case QEvent::TouchEnd:
        case QEvent::TouchCancel:
            return handleTouchEnd(static_cast<QTouchEvent*>(event));
        case QEvent::Wheel:
            restartCameraResetTimer();
            break;
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::RightButton) {
                restartCameraResetTimer();
            }
            break;
        }
        case QEvent::MouseMove: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->buttons().testFlag(Qt::RightButton)) {
                restartCameraResetTimer();
            }
            break;
        }
        default:
            break;
        }

        if (m_touchSequenceActive && shouldBlockMouseEvent(event->type())) {
            event->accept();
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

bool VtkSceneWindow::handleTouchBegin(QTouchEvent* event)
{
    if (event == nullptr) {
        return false;
    }

    const QVector<QPointF> positions = extractTouchPositions(event);
    m_touchSequenceActive = !positions.isEmpty();

    if (positions.size() == 1) {
        m_isPinching = false;
        m_initialPinchDistance = 0.0;
        m_isRotating = true;
        m_lastRotatePos = positions[0];
    } else if (positions.size() == 2) {
        m_isRotating = false;
        m_isPinching = true;
        m_initialPinchDistance = calculateDistance(positions[0], positions[1]);
        m_lastPinchCenter = pinchCenter(positions[0], positions[1]);

        if (m_camera) {
            m_currentCameraDistance = m_camera->GetDistance();
            m_currentParallelScale = m_camera->GetParallelScale();
        }
    } else {
        m_isRotating = false;
        m_isPinching = false;
        m_initialPinchDistance = 0.0;
    }

    event->accept();
    return true;
}

bool VtkSceneWindow::handleTouchUpdate(QTouchEvent* event)
{
    if (event == nullptr) {
        return false;
    }

    const QVector<QPointF> positions = extractTouchPositions(event);
    m_touchSequenceActive = !positions.isEmpty();

    if (positions.size() == 1) {
        const QPointF& currentPos = positions[0];

        if (m_isPinching) {
            m_isPinching = false;
            m_initialPinchDistance = 0.0;
            m_isRotating = true;
            m_lastRotatePos = currentPos;
            event->accept();
            return true;
        }

        if (!m_isRotating) {
            m_isRotating = true;
            m_lastRotatePos = currentPos;
            event->accept();
            return true;
        }

        const double deltaX = currentPos.x() - m_lastRotatePos.x();
        const double deltaY = currentPos.y() - m_lastRotatePos.y();
        if ((deltaX != 0.0 || deltaY != 0.0) && m_camera != nullptr) {
            rotateCamera(deltaX, deltaY);
            m_lastRotatePos = currentPos;
            renderAfterTouch();
        }

        event->accept();
        return true;
    }

    if (positions.size() == 2) {
        const QPointF& point1 = positions[0];
        const QPointF& point2 = positions[1];
        const double currentDistance = calculateDistance(point1, point2);
        const QPointF currentCenter = pinchCenter(point1, point2);

        if (!m_isPinching) {
            m_isPinching = true;
            m_isRotating = false;
            m_initialPinchDistance = currentDistance;
            m_lastPinchCenter = currentCenter;

            if (m_camera) {
                m_currentCameraDistance = m_camera->GetDistance();
                m_currentParallelScale = m_camera->GetParallelScale();
            }

            event->accept();
            return true;
        }

        if (m_camera != nullptr) {
            if (m_initialPinchDistance > 0.0 && currentDistance > 1e-9) {
                const double zoomFactor = m_initialPinchDistance / currentDistance;

                if (m_camera->GetParallelProjection()) {
                    m_camera->SetParallelScale(
                        std::max(1e-6, m_currentParallelScale * zoomFactor));
                } else {
                    double focalPoint[3] = {0.0, 0.0, 0.0};
                    double position[3] = {0.0, 0.0, 0.0};
                    m_camera->GetFocalPoint(focalPoint);
                    m_camera->GetPosition(position);

                    double direction[3] = {
                        position[0] - focalPoint[0],
                        position[1] - focalPoint[1],
                        position[2] - focalPoint[2]
                    };
                    const double norm = std::sqrt(direction[0] * direction[0]
                                                  + direction[1] * direction[1]
                                                  + direction[2] * direction[2]);
                    if (norm > 1e-9) {
                        direction[0] /= norm;
                        direction[1] /= norm;
                        direction[2] /= norm;

                        const double newDistance =
                            std::max(1e-6, m_currentCameraDistance * zoomFactor);
                        m_camera->SetPosition(focalPoint[0] + direction[0] * newDistance,
                                              focalPoint[1] + direction[1] * newDistance,
                                              focalPoint[2] + direction[2] * newDistance);
                    }
                }

                if (m_renderers[0]) {
                    m_renderers[0]->ResetCameraClippingRange();
                }
            }

            const double deltaX = currentCenter.x() - m_lastPinchCenter.x();
            const double deltaY = currentCenter.y() - m_lastPinchCenter.y();
            if (deltaX != 0.0 || deltaY != 0.0) {
                panCamera(deltaX, deltaY);
                m_lastPinchCenter = currentCenter;
            }

            renderAfterTouch();
        }

        event->accept();
        return true;
    }

    m_isPinching = false;
    m_isRotating = false;
    m_initialPinchDistance = 0.0;
    event->accept();
    return true;
}

bool VtkSceneWindow::handleTouchEnd(QTouchEvent* event)
{
    if (event == nullptr) {
        return false;
    }

    const QVector<QPointF> positions = extractTouchPositions(event);

    if (event->type() == QEvent::TouchCancel || positions.isEmpty()) {
        resetTouchState();
        event->accept();
        return true;
    }

    m_isPinching = false;
    m_initialPinchDistance = 0.0;
    m_currentCameraDistance = 0.0;
    m_currentParallelScale = 1.0;

    if (positions.size() == 1) {
        m_touchSequenceActive = true;
        m_isRotating = true;
        m_lastRotatePos = positions[0];
    } else {
        m_isRotating = false;
        m_touchSequenceActive = false;
    }

    event->accept();
    return true;
}

void VtkSceneWindow::rotateCamera(double deltaX, double deltaY)
{
    if (m_camera == nullptr || m_vtkWidget == nullptr) {
        return;
    }

    const int width = std::max(1, m_vtkWidget->width());
    const int height = std::max(1, m_vtkWidget->height());
    const double azimuthDeg = -deltaX * 180.0 / static_cast<double>(width);
    const double elevationDeg = deltaY * 180.0 / static_cast<double>(height);

    m_camera->Azimuth(azimuthDeg);
    m_camera->Elevation(elevationDeg);
    m_camera->OrthogonalizeViewUp();

    if (m_renderers[0]) {
        m_renderers[0]->ResetCameraClippingRange();
    }
}

void VtkSceneWindow::panCamera(double deltaX, double deltaY)
{
    if (m_camera == nullptr) {
        return;
    }

    double position[3] = {0.0, 0.0, 0.0};
    double focalPoint[3] = {0.0, 0.0, 0.0};
    double viewUp[3] = {0.0, 0.0, 0.0};
    m_camera->GetPosition(position);
    m_camera->GetFocalPoint(focalPoint);
    m_camera->GetViewUp(viewUp);

    double vpn[3] = {
        position[0] - focalPoint[0],
        position[1] - focalPoint[1],
        position[2] - focalPoint[2]
    };
    double right[3] = {0.0, 0.0, 0.0};
    vtkMath::Cross(vpn, viewUp, right);

    if (vtkMath::Normalize(right) <= 0.0 || vtkMath::Normalize(viewUp) <= 0.0) {
        return;
    }

    const double factor = m_camera->GetParallelProjection()
        ? std::max(1e-6, m_camera->GetParallelScale() / 250.0)
        : std::max(1e-6, m_camera->GetDistance() / 500.0);

    const double motionVector[3] = {
        deltaX * factor * right[0] + deltaY * factor * viewUp[0],
        deltaX * factor * right[1] + deltaY * factor * viewUp[1],
        deltaX * factor * right[2] + deltaY * factor * viewUp[2]
    };

    m_camera->SetPosition(position[0] + motionVector[0],
                          position[1] + motionVector[1],
                          position[2] + motionVector[2]);
    m_camera->SetFocalPoint(focalPoint[0] + motionVector[0],
                            focalPoint[1] + motionVector[1],
                            focalPoint[2] + motionVector[2]);

    if (m_renderers[0]) {
        m_renderers[0]->ResetCameraClippingRange();
    }
}

double VtkSceneWindow::calculateDistance(const QPointF& point1, const QPointF& point2) const
{
    const double deltaX = point2.x() - point1.x();
    const double deltaY = point2.y() - point1.y();
    return std::sqrt(deltaX * deltaX + deltaY * deltaY);
}

void VtkSceneWindow::renderAfterTouch()
{
    if (m_isShuttingDown || m_renderWindow == nullptr) {
        return;
    }

    restartCameraResetTimer();

    m_renderWindow->Render();
}

void VtkSceneWindow::restartCameraResetTimer()
{
    if (m_isShuttingDown || m_cameraResetTimer == nullptr) {
        return;
    }

    m_cameraResetTimer->start();
}

void VtkSceneWindow::resetTouchState()
{
    m_touchSequenceActive = false;
    m_isPinching = false;
    m_isRotating = false;
    m_initialPinchDistance = 0.0;
    m_currentCameraDistance = 0.0;
    m_currentParallelScale = 1.0;
    m_lastPinchCenter = QPointF();
    m_lastRotatePos = QPointF();
}

bool VtkSceneWindow::shouldBlockMouseEvent(QEvent::Type eventType) const
{
    switch (eventType) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonRelease:
    case QEvent::MouseButtonDblClick:
    case QEvent::MouseMove:
        return true;
    default:
        return false;
    }
}
