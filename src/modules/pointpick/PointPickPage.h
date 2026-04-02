#pragma once

#include <QWidget>
#include <QStringList>

namespace Ui {
class PointPickPage;
}

class PointPickPage : public QWidget
{
    Q_OBJECT

public:
    explicit PointPickPage(QWidget* parent = nullptr);
    ~PointPickPage() override;

signals:
    void confirmPointsRequested();
    void datagenLineCreateRequested();

public slots:
    void updatePointList(const QStringList& points);
    void setPointCount(int count);
    void setConfirmed(bool confirmed);

private:
    Ui::PointPickPage* m_ui = nullptr;
};
