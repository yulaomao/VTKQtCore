#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;
class VtkSceneWindow;
class UiActionDispatcher;

class PlanningPage : public QWidget
{
    Q_OBJECT

public:
    explicit PlanningPage(QWidget* parent = nullptr);

    void setActionDispatcher(UiActionDispatcher* dispatcher);
    void setSceneWindow(VtkSceneWindow* sceneWindow);
    void setSecondarySceneWindow(VtkSceneWindow* sceneWindow);

public slots:
    void setPlanStatus(const QString& status);

private:
    UiActionDispatcher* m_actionDispatcher = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_generateButton = nullptr;
    QPushButton* m_acceptButton = nullptr;
    QPushButton* m_datagenTestButton = nullptr;
    QVBoxLayout* m_sceneLayout = nullptr;
    QVBoxLayout* m_secondarySceneLayout = nullptr;
    VtkSceneWindow* m_sceneWindow = nullptr;
    VtkSceneWindow* m_secondarySceneWindow = nullptr;
};
