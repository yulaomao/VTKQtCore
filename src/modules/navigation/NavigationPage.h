#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;
class UiActionDispatcher;

class NavigationPage : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationPage(QWidget* parent = nullptr);

    void setActionDispatcher(UiActionDispatcher* dispatcher);
    void setSceneWindow(QWidget* sceneWindow);

public slots:
    void setNavigationStatus(const QString& status);
    void setCurrentPosition(double x, double y, double z);
    void setNavigating(bool navigating);

private:
    UiActionDispatcher* m_actionDispatcher = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QPushButton* m_datagenTestBtn = nullptr;
    QLabel* m_positionLabel = nullptr;
    QVBoxLayout* m_sceneLayout = nullptr;
    QWidget* m_sceneWindow = nullptr;
};
