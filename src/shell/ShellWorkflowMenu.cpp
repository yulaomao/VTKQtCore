#include "ShellWorkflowMenu.h"

#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

ShellWorkflowMenu::ShellWorkflowMenu(QWidget* parent)
    : QWidget(parent)
    , m_buttonLayout(new QVBoxLayout)
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(8);

    auto* title = new QLabel(QStringLiteral("Workflow"), this);
    title->setStyleSheet(QStringLiteral("font-weight: 600;"));
    rootLayout->addWidget(title);

    m_buttonLayout->setContentsMargins(0, 0, 0, 0);
    m_buttonLayout->setSpacing(6);
    rootLayout->addLayout(m_buttonLayout);
    rootLayout->addStretch(1);
}

void ShellWorkflowMenu::setWorkflowSequence(const QStringList& modules)
{
    m_modules = modules;
    rebuildButtons();
    refreshButtonState();
}

void ShellWorkflowMenu::setCurrentModule(const QString& moduleId)
{
    m_currentModule = moduleId;
    refreshButtonState();
}

void ShellWorkflowMenu::setEnterableModules(const QStringList& moduleIds)
{
    m_enterableModules.clear();
    for (const QString& moduleId : moduleIds) {
        m_enterableModules.insert(moduleId);
    }
    refreshButtonState();
}

void ShellWorkflowMenu::rebuildButtons()
{
    while (QLayoutItem* item = m_buttonLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    m_buttons.clear();

    for (const QString& moduleId : m_modules) {
        auto* button = new QPushButton(moduleId, this);
        button->setCheckable(true);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(button, &QPushButton::clicked, this, [this, moduleId]() {
            emit moduleSelected(moduleId);
        });
        m_buttonLayout->addWidget(button);
        m_buttons.insert(moduleId, button);
    }
}

void ShellWorkflowMenu::refreshButtonState()
{
    for (auto it = m_buttons.begin(); it != m_buttons.end(); ++it) {
        const bool current = it.key() == m_currentModule;
        const bool enterable = m_enterableModules.isEmpty() ||
                               m_enterableModules.contains(it.key());
        it.value()->setEnabled(enterable);
        it.value()->setChecked(current);
    }
}