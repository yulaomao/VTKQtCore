#include "GlobalUiManager.h"
#include "ui/vtk3d/VtkSceneWindow.h"

#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

GlobalUiManager::GlobalUiManager(QObject* parent)
    : QObject(parent)
    , m_overlayLayer(nullptr)
    , m_toolHost(nullptr)
    , m_notificationWidget(nullptr)
    , m_overlayWidget(nullptr)
{
}

void GlobalUiManager::setOverlayLayer(QWidget* overlay)
{
    m_overlayLayer = overlay;
    updateOverlayLayerState();
}

void GlobalUiManager::setToolHost(QWidget* toolHost)
{
    m_toolHost = toolHost;
    ensureToolHostLayout();
    if (m_toolHost) {
        m_toolHost->hide();
    }
}

void GlobalUiManager::showNotification(const QString& message, const QString& level)
{
    hideNotification();

    if (!m_overlayLayer) {
        return;
    }

    auto* label = new QLabel(message, m_overlayLayer);
    label->setObjectName("globalNotification");
    label->setAlignment(Qt::AlignCenter);
    label->setWordWrap(true);

    if (level == "warning") {
        label->setStyleSheet(
            "background-color: rgba(255, 165, 0, 200); color: white; "
            "padding: 10px; border-radius: 4px;");
    } else if (level == "error") {
        label->setStyleSheet(
            "background-color: rgba(220, 50, 50, 200); color: white; "
            "padding: 10px; border-radius: 4px;");
    } else {
        label->setStyleSheet(
            "background-color: rgba(50, 130, 200, 200); color: white; "
            "padding: 10px; border-radius: 4px;");
    }

    label->adjustSize();
    label->move((m_overlayLayer->width() - label->width()) / 2, 10);
    label->show();
    label->raise();

    m_notificationWidget = label;
    updateOverlayLayerState();
}

void GlobalUiManager::hideNotification()
{
    if (m_notificationWidget) {
        m_notificationWidget->hide();
        m_notificationWidget->deleteLater();
        m_notificationWidget = nullptr;
    }

    updateOverlayLayerState();
}

void GlobalUiManager::showConfirmation(const QString& title, const QString& message,
                                       std::function<void(bool)> callback)
{
    auto result = QMessageBox::question(m_overlayLayer, title, message,
                                        QMessageBox::Yes | QMessageBox::No,
                                        QMessageBox::No);
    if (callback) {
        callback(result == QMessageBox::Yes);
    }
}

void GlobalUiManager::showOverlay(const QString& message)
{
    hideOverlay();

    if (!m_overlayLayer) {
        return;
    }

    m_overlayWidget = new QWidget(m_overlayLayer);
    m_overlayWidget->setStyleSheet("background-color: rgba(0, 0, 0, 128);");
    m_overlayWidget->setGeometry(m_overlayLayer->rect());

    if (!message.isEmpty()) {
        auto* layout = new QVBoxLayout(m_overlayWidget);
        auto* label = new QLabel(message, m_overlayWidget);
        label->setAlignment(Qt::AlignCenter);
        label->setStyleSheet("color: white; font-size: 18px;");
        layout->addWidget(label);
    }

    m_overlayWidget->show();
    m_overlayWidget->raise();
    updateOverlayLayerState();
}

void GlobalUiManager::hideOverlay()
{
    if (m_overlayWidget) {
        m_overlayWidget->hide();
        m_overlayWidget->deleteLater();
        m_overlayWidget = nullptr;
    }

    updateOverlayLayerState();
}

void GlobalUiManager::registerVtkWindow(VtkSceneWindow* window)
{
    if (window) {
        m_vtkWindows.insert(window->getWindowId(), window);
    }
}

void GlobalUiManager::unregisterVtkWindow(const QString& windowId)
{
    m_vtkWindows.remove(windowId);
}

VtkSceneWindow* GlobalUiManager::getVtkWindow(const QString& windowId) const
{
    return m_vtkWindows.value(windowId, nullptr);
}

void GlobalUiManager::showError(const QString& errorCode, const QString& message,
                                bool recoverable, const QString& suggestedAction)
{
    QString detail = QString("[%1] %2").arg(errorCode, message);
    if (!suggestedAction.isEmpty()) {
        detail += QString("\n\nSuggested action: %1").arg(suggestedAction);
    }
    if (!recoverable) {
        detail += "\n\nThis error is non-recoverable. The application may need to restart.";
    }

    QMessageBox::critical(m_overlayLayer, "Error", detail);
}

void GlobalUiManager::ensureToolHostLayout()
{
    if (!m_toolHost || m_toolHost->layout()) {
        return;
    }

    auto* layout = new QVBoxLayout(m_toolHost);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(8);
}

void GlobalUiManager::updateOverlayLayerState()
{
    if (!m_overlayLayer) {
        return;
    }

    const bool hasOverlay = m_overlayWidget != nullptr;
    const bool hasNotification = m_notificationWidget != nullptr;
    const bool visible = hasOverlay || hasNotification;

    m_overlayLayer->setVisible(visible);
    m_overlayLayer->setAttribute(Qt::WA_TransparentForMouseEvents,
                                 hasNotification && !hasOverlay);

    if (visible) {
        m_overlayLayer->raise();
    }
}
