#include "ShellStatusBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

ShellStatusBar::ShellStatusBar(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    m_prevButton = new QPushButton(QStringLiteral("Prev"), this);
    m_nextButton = new QPushButton(QStringLiteral("Next"), this);
    auto* resyncButton = new QPushButton(QStringLiteral("Resync"), this);
    m_stepLabel = new QLabel(QStringLiteral("No active module"), this);
    m_connectionLabel = new QLabel(QStringLiteral("Disconnected"), this);
    m_healthLabel = new QLabel(QStringLiteral("Health: offline"), this);

    layout->addWidget(m_prevButton);
    layout->addWidget(m_nextButton);
    layout->addWidget(m_stepLabel, 1);
    layout->addWidget(m_connectionLabel);
    layout->addWidget(m_healthLabel);
    layout->addWidget(resyncButton);

    connect(m_prevButton, &QPushButton::clicked, this, &ShellStatusBar::prevRequested);
    connect(m_nextButton, &QPushButton::clicked, this, &ShellStatusBar::nextRequested);
    connect(resyncButton, &QPushButton::clicked, this, [this]() {
        emit resyncRequested(QStringLiteral("shell_status_bar"));
    });
}

void ShellStatusBar::setWorkflowSequence(const QStringList& modules)
{
    m_modules = modules;
    refreshState();
}

void ShellStatusBar::setCurrentModule(const QString& moduleId)
{
    m_currentModule = moduleId;
    refreshState();
}

void ShellStatusBar::setEnterableModules(const QStringList& moduleIds)
{
    m_enterableModules.clear();
    for (const QString& moduleId : moduleIds) {
        m_enterableModules.insert(moduleId);
    }
    refreshState();
}

void ShellStatusBar::setConnectionState(const QString& state)
{
    m_connectionState = state;
    refreshState();
}

void ShellStatusBar::setHealthSnapshot(const QVariantMap& snapshot)
{
    const QString healthState = snapshot.value(QStringLiteral("healthState")).toString();
    if (!healthState.isEmpty()) {
        m_healthState = healthState;
    }
    refreshState();
}

void ShellStatusBar::setWorkflowDecision(const QString& reasonCode, const QString& message)
{
    m_workflowReasonCode = reasonCode;
    m_workflowReasonMessage = message;
    refreshState();
}

void ShellStatusBar::refreshState()
{
    const int currentIndex = m_modules.indexOf(m_currentModule);
    const int stepNumber = currentIndex >= 0 ? currentIndex + 1 : 0;
    const int totalSteps = m_modules.size();
    const QString stepText = currentIndex >= 0
        ? QStringLiteral("Step %1/%2  %3").arg(stepNumber).arg(totalSteps).arg(m_currentModule)
        : QStringLiteral("No active module");
    QString decoratedStepText = stepText;
    if (!m_workflowReasonCode.isEmpty()) {
        decoratedStepText += QStringLiteral("  [%1]").arg(m_workflowReasonCode);
    }
    m_stepLabel->setText(decoratedStepText);
    m_connectionLabel->setText(QStringLiteral("Connection: %1").arg(m_connectionState));
    if (!m_workflowReasonMessage.isEmpty()) {
        m_healthLabel->setText(
            QStringLiteral("Health: %1 | %2").arg(m_healthState, m_workflowReasonMessage));
    } else {
        m_healthLabel->setText(QStringLiteral("Health: %1").arg(m_healthState));
    }

    QString prevModule;
    QString nextModule;
    if (currentIndex >= 0) {
        if (currentIndex > 0) {
            prevModule = m_modules.at(currentIndex - 1);
        }
        if (currentIndex + 1 < totalSteps) {
            nextModule = m_modules.at(currentIndex + 1);
        }
    }

    const bool prevEnabled = !prevModule.isEmpty() &&
        (m_enterableModules.isEmpty() || m_enterableModules.contains(prevModule));
    const bool nextEnabled = !nextModule.isEmpty() &&
        (m_enterableModules.isEmpty() || m_enterableModules.contains(nextModule));

    m_prevButton->setEnabled(prevEnabled);
    m_nextButton->setEnabled(nextEnabled);
}