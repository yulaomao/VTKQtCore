#include "WorkspaceShell.h"

#include <QVBoxLayout>
#include <QHBoxLayout>

namespace {

bool hasHostedWidgets(const QWidget* host)
{
    return host && host->layout() && host->layout()->count() > 0;
}

}

WorkspaceShell::WorkspaceShell(QWidget* parent)
    : QWidget(parent)
{
    m_topWidget = new QWidget(this);
    m_topWidget->setFixedHeight(48);
    m_topWidget->setLayout(new QHBoxLayout);
    m_topWidget->layout()->setContentsMargins(8, 8, 8, 4);

    m_centerStack = new QStackedWidget(this);

    m_rightWidget = new QWidget(this);
    m_rightWidget->setFixedWidth(280);
    auto* rightLayout = new QVBoxLayout(m_rightWidget);
    rightLayout->setContentsMargins(8, 8, 8, 8);
    rightLayout->setSpacing(8);
    m_rightShellHost = new QWidget(m_rightWidget);
    m_rightShellHost->setLayout(new QVBoxLayout);
    m_rightShellHost->layout()->setContentsMargins(0, 0, 0, 0);
    m_rightShellHost->layout()->setSpacing(8);
    m_rightAuxiliaryHost = new QWidget(m_rightWidget);
    m_rightAuxiliaryHost->setLayout(new QVBoxLayout);
    m_rightAuxiliaryHost->layout()->setContentsMargins(0, 0, 0, 0);
    m_rightAuxiliaryHost->layout()->setSpacing(8);
    rightLayout->addWidget(m_rightShellHost);
    rightLayout->addWidget(m_rightAuxiliaryHost);
    rightLayout->addStretch(1);

    m_bottomWidget = new QWidget(this);
    m_bottomWidget->setFixedHeight(52);
    auto* bottomLayout = new QHBoxLayout(m_bottomWidget);
    bottomLayout->setContentsMargins(8, 4, 8, 8);
    bottomLayout->setSpacing(8);
    m_bottomShellHost = new QWidget(m_bottomWidget);
    m_bottomShellHost->setLayout(new QHBoxLayout);
    m_bottomShellHost->layout()->setContentsMargins(0, 0, 0, 0);
    m_bottomShellHost->layout()->setSpacing(8);
    m_bottomAuxiliaryHost = new QWidget(m_bottomWidget);
    m_bottomAuxiliaryHost->setLayout(new QHBoxLayout);
    m_bottomAuxiliaryHost->layout()->setContentsMargins(0, 0, 0, 0);
    m_bottomAuxiliaryHost->layout()->setSpacing(8);
    bottomLayout->addWidget(m_bottomShellHost, 1);
    bottomLayout->addWidget(m_bottomAuxiliaryHost, 0);

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

    updateAuxiliaryVisibility();
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

QWidget* WorkspaceShell::getRightShellHost() const
{
    return m_rightShellHost;
}

QWidget* WorkspaceShell::getBottomShellHost() const
{
    return m_bottomShellHost;
}

void WorkspaceShell::refreshHostVisibility()
{
    updateAuxiliaryVisibility();
}

void WorkspaceShell::mountRightAuxiliary(QWidget* widget)
{
    if (!widget) {
        return;
    }
    widget->setParent(m_rightAuxiliaryHost);
    m_rightAuxiliaryHost->layout()->addWidget(widget);
    widget->show();
    updateAuxiliaryVisibility();
}

void WorkspaceShell::unmountRightAuxiliary(QWidget* widget)
{
    if (!widget) {
        return;
    }
    m_rightAuxiliaryHost->layout()->removeWidget(widget);
    widget->hide();
    updateAuxiliaryVisibility();
}

void WorkspaceShell::clearRightAuxiliary()
{
    QLayout* layout = m_rightAuxiliaryHost->layout();
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
        }
        delete item;
    }
    updateAuxiliaryVisibility();
}

void WorkspaceShell::mountBottomAuxiliary(QWidget* widget)
{
    if (!widget) {
        return;
    }
    widget->setParent(m_bottomAuxiliaryHost);
    m_bottomAuxiliaryHost->layout()->addWidget(widget);
    widget->show();
    updateAuxiliaryVisibility();
}

void WorkspaceShell::unmountBottomAuxiliary(QWidget* widget)
{
    if (!widget) {
        return;
    }
    m_bottomAuxiliaryHost->layout()->removeWidget(widget);
    widget->hide();
    updateAuxiliaryVisibility();
}

void WorkspaceShell::clearBottomAuxiliary()
{
    QLayout* layout = m_bottomAuxiliaryHost->layout();
    while (QLayoutItem* item = layout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->hide();
        }
        delete item;
    }
    updateAuxiliaryVisibility();
}

void WorkspaceShell::updateAuxiliaryVisibility()
{
    m_rightWidget->setVisible(hasHostedWidgets(m_rightShellHost) ||
                              hasHostedWidgets(m_rightAuxiliaryHost));
    m_bottomWidget->setVisible(hasHostedWidgets(m_bottomShellHost) ||
                               hasHostedWidgets(m_bottomAuxiliaryHost));
}
