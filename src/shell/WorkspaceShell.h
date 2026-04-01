#pragma once

#include <QWidget>
#include <QStackedWidget>

class WorkspaceShell : public QWidget
{
    Q_OBJECT

public:
    explicit WorkspaceShell(QWidget* parent = nullptr);
    ~WorkspaceShell() override = default;

    QStackedWidget* getCenterStack() const;
    QWidget* getTopWidget() const;
    QWidget* getRightWidget() const;
    QWidget* getBottomWidget() const;

    void mountRightAuxiliary(QWidget* widget);
    void unmountRightAuxiliary(QWidget* widget);
    void clearRightAuxiliary();

    void mountBottomAuxiliary(QWidget* widget);
    void unmountBottomAuxiliary(QWidget* widget);
    void clearBottomAuxiliary();

private:
    QWidget* m_topWidget;
    QStackedWidget* m_centerStack;
    QWidget* m_rightWidget;
    QWidget* m_bottomWidget;
};
