#pragma once

#include <QWidget>

namespace Ui {
class PointPickStatusPanel;
}

class PointPickStatusPanel : public QWidget
{
    Q_OBJECT

public:
    explicit PointPickStatusPanel(QWidget* parent = nullptr);
    ~PointPickStatusPanel() override;

public slots:
    void setPointCount(int count);
    void setConfirmed(bool confirmed);

private:
    Ui::PointPickStatusPanel* m_ui = nullptr;
};