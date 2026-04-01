#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;
class QWidget;

class NavigationPage : public QWidget
{
    Q_OBJECT

public:
    explicit NavigationPage(QWidget* parent = nullptr);

    void setSceneWindow(QWidget* sceneWindow);

signals:
    void startNavigationRequested();
    void stopNavigationRequested();

public slots:
    void setNavigationStatus(const QString& status);
    void setCurrentPosition(double x, double y, double z);
    void setNavigating(bool navigating);

private:
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QLabel* m_positionLabel = nullptr;
    QVBoxLayout* m_sceneLayout = nullptr;
    QWidget* m_sceneWindow = nullptr;
};
