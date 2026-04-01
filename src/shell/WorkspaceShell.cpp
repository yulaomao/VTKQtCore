#include "WorkspaceShell.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

WorkspaceShell::WorkspaceShell(QWidget* parent)
    : QWidget(parent)
{
    m_topWidget = new QWidget(this);
    m_topWidget->setFixedHeight(40);
    m_topWidget->setLayout(new QHBoxLayout);
    m_topWidget->layout()->setContentsMargins(0, 0, 0, 0);

    m_centerStack = new QStackedWidget(this);

    m_rightWidget = new QWidget(this);
    m_rightWidget->setFixedWidth(250);
    auto* rightLayout = new QVBoxLayout(m_rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);

    m_bottomWidget = new QWidget(this);
    m_bottomWidget->setFixedHeight(30);
    auto* bottomLayout = new QHBoxLayout(m_bottomWidget);
    bottomLayout->setContentsMargins(0, 0, 0, 0);

    auto* middleLayout = new QHBoxLayout;
    middleLayout->setContentsMargins(0, 0, 0, 0);
    middleLayout->setSpacing(0);
    middleLayout->addWidget(m_centerStack, 1);
    middleLayout->addWidget(m_rightWidget);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_topWidget);
    mainLayout->addLayout(middleLayout, 1);
    mainLayout->addWidget(m_bottomWidget);
}

QStackedWidget* WorkspaceShell::getCenterStack() const
{
    return m_centerStack;
}

QWidget* WorkspaceShell::getTopWidget() const
{
    return m_topWidget;
}

QWidget* WorkspaceShell::getRightWidget() const
{
    return m_rightWidget;
}

QWidget* WorkspaceShell::getBottomWidget() const
{
    return m_bottomWidget;
}

void WorkspaceShell::mountRightAuxiliary(QWidget* widget)
{
    if (!widget) {
        return;
    }
    widget->setParent(m_rightWidget);
    m_rightWidget->layout()->addWidget(widget);
    widget->show();
}

void WorkspaceShell::unmountRightAuxiliary(QWidget* widget)
{
    if (!widget) {
        return;
    }
    m_rightWidget->layout()->removeWidget(widget);
    widget->hide();
}

void WorkspaceShell::clearRightAuxiliary()
{
    QLayout* layout = m_rightWidget->layout();
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
        }
        delete item;
    }
}

void WorkspaceShell::mountBottomAuxiliary(QWidget* widget)
{
    if (!widget) {
        return;
    }
    widget->setParent(m_bottomWidget);
    m_bottomWidget->layout()->addWidget(widget);
    widget->show();
}

void WorkspaceShell::unmountBottomAuxiliary(QWidget* widget)
{
    if (!widget) {
        return;
    }
    m_bottomWidget->layout()->removeWidget(widget);
    widget->hide();
}

void WorkspaceShell::clearBottomAuxiliary()
{
    QLayout* layout = m_bottomWidget->layout();
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
        }
        delete item;
    }
}
