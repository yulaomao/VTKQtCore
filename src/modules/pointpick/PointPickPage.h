#pragma once

#include <QWidget>
#include <QStringList>

class UiActionDispatcher;

namespace Ui {
class PointPickPage;
}

class PointPickPage : public QWidget
{
    Q_OBJECT

public:
    explicit PointPickPage(QWidget* parent = nullptr);
    ~PointPickPage() override;

    void setActionDispatcher(UiActionDispatcher* dispatcher);

public slots:
    void updatePointList(const QStringList& points);
    void setPointCount(int count);
    void setConfirmed(bool confirmed);

private:
    UiActionDispatcher* m_actionDispatcher = nullptr;
    Ui::PointPickPage* m_ui = nullptr;
};
