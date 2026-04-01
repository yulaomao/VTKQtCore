#pragma once

#include <QWidget>
#include <QStringList>

class QLabel;
class QListWidget;
class QPushButton;

class PointPickPage : public QWidget
{
    Q_OBJECT

public:
    explicit PointPickPage(QWidget* parent = nullptr);

signals:
    void confirmPointsRequested();

public slots:
    void updatePointList(const QStringList& points);
    void setPointCount(int count);
    void setConfirmed(bool confirmed);

private:
    QLabel* m_pointCountLabel = nullptr;
    QListWidget* m_pointList = nullptr;
    QPushButton* m_confirmButton = nullptr;
    QLabel* m_statusLabel = nullptr;
};
