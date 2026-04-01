#include "PlanningPage.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

PlanningPage::PlanningPage(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* titleLabel = new QLabel(QStringLiteral("Surgical Planning"), this);
    mainLayout->addWidget(titleLabel);

    m_statusLabel = new QLabel(QStringLiteral("Plan Status: Not Started"), this);
    mainLayout->addWidget(m_statusLabel);

    m_generateButton = new QPushButton(QStringLiteral("Generate Plan"), this);
    mainLayout->addWidget(m_generateButton);

    m_acceptButton = new QPushButton(QStringLiteral("Accept Plan"), this);
    mainLayout->addWidget(m_acceptButton);

    mainLayout->addStretch();

    connect(m_generateButton, &QPushButton::clicked,
            this, &PlanningPage::generatePlanRequested);
    connect(m_acceptButton, &QPushButton::clicked,
            this, &PlanningPage::acceptPlanRequested);
}

void PlanningPage::setPlanStatus(const QString& status)
{
    m_statusLabel->setText(QStringLiteral("Plan Status: %1").arg(status));
}
