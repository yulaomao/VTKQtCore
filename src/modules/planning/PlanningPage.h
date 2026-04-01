#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

class PlanningPage : public QWidget
{
    Q_OBJECT

public:
    explicit PlanningPage(QWidget* parent = nullptr);

signals:
    void generatePlanRequested();
    void acceptPlanRequested();

public slots:
    void setPlanStatus(const QString& status);

private:
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_generateButton = nullptr;
    QPushButton* m_acceptButton = nullptr;
};
