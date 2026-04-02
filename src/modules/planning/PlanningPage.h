#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class QVBoxLayout;
class VtkSceneWindow;

class PlanningPage : public QWidget
{
    Q_OBJECT

public:
    explicit PlanningPage(QWidget* parent = nullptr);

    void setSceneWindow(VtkSceneWindow* sceneWindow);
    void setSecondarySceneWindow(VtkSceneWindow* sceneWindow);

signals:
    void generatePlanRequested();
    void acceptPlanRequested();
    void datagenModelCreateRequested();

public slots:
    void setPlanStatus(const QString& status);

private:
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_generateButton = nullptr;
    QPushButton* m_acceptButton = nullptr;
    QPushButton* m_datagenTestButton = nullptr;
    QVBoxLayout* m_sceneLayout = nullptr;
    QVBoxLayout* m_secondarySceneLayout = nullptr;
    VtkSceneWindow* m_sceneWindow = nullptr;
    VtkSceneWindow* m_secondarySceneWindow = nullptr;
};
