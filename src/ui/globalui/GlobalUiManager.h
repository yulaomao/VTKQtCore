#pragma once

#include <QObject>
#include <QWidget>
#include <QMap>
#include <QString>
#include <functional>

class VtkSceneWindow;

class GlobalUiManager : public QObject
{
    Q_OBJECT

public:
    explicit GlobalUiManager(QObject* parent = nullptr);
    ~GlobalUiManager() override = default;

    void setOverlayLayer(QWidget* overlay);
    void setToolHost(QWidget* toolHost);

    // Global notifications
    void showNotification(const QString& message, const QString& level = "info");
    void hideNotification();

    // Confirmation dialog
    void showConfirmation(const QString& title, const QString& message,
                          std::function<void(bool)> callback);

    // Overlay/mask
    void showOverlay(const QString& message = "");
    void hideOverlay();

    // 3D tool windows
    void registerVtkWindow(VtkSceneWindow* window);
    void unregisterVtkWindow(const QString& windowId);
    VtkSceneWindow* getVtkWindow(const QString& windowId) const;

    // Error display
    void showError(const QString& errorCode, const QString& message,
                   bool recoverable, const QString& suggestedAction);

private:
    QWidget* m_overlayLayer;
    QWidget* m_toolHost;
    QMap<QString, VtkSceneWindow*> m_vtkWindows;
    QWidget* m_notificationWidget;
    QWidget* m_overlayWidget;
};
