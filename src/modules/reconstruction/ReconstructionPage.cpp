#include "ReconstructionPage.h"

#include "ReconstructionUiCommands.h"
#include "ui/coordination/UiActionDispatcher.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

ReconstructionPage::ReconstructionPage(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* titleLabel = new QLabel(QStringLiteral(" Reconstruction"), this);
    titleLabel->setStyleSheet(QStringLiteral("font-weight: 600;"));
    mainLayout->addWidget(titleLabel);

    m_startButton = new QPushButton(QStringLiteral("开始重建"), this);
    mainLayout->addWidget(m_startButton);

    m_resetButton = new QPushButton(QStringLiteral("重置重建"), this);
    mainLayout->addWidget(m_resetButton);

    m_statusLabel = new QLabel(QStringLiteral("状态: 等待操作"), this);
    mainLayout->addWidget(m_statusLabel);

    mainLayout->addStretch();

    connect(m_startButton, &QPushButton::clicked, this, [this]() {
        if (!m_actionDispatcher) {
            return;
        }

        m_actionDispatcher->sendCommand(
            ReconstructionUiCommands::startReconstruction());
    });

    connect(m_resetButton, &QPushButton::clicked, this, [this]() {
        if (!m_actionDispatcher) {
            return;
        }

        m_actionDispatcher->sendCommand(
            ReconstructionUiCommands::resetReconstruction());
    });
}

void ReconstructionPage::setActionDispatcher(UiActionDispatcher* dispatcher)
{
    m_actionDispatcher = dispatcher;
}

void ReconstructionPage::setReconstructionStatus(const QString& status, bool done)
{
    QString text = QStringLiteral("状态: %1").arg(status);
    if (done) {
        text += QStringLiteral(" (已完成)");
    }
    m_statusLabel->setText(text);
}
