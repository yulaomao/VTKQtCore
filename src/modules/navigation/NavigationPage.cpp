#include "NavigationPage.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>

NavigationPage::NavigationPage(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* titleLabel = new QLabel(QStringLiteral("Surgical Navigation"), this);
    mainLayout->addWidget(titleLabel);

    m_statusLabel = new QLabel(QStringLiteral("Status: Idle"), this);
    mainLayout->addWidget(m_statusLabel);

    m_startBtn = new QPushButton(QStringLiteral("Start Navigation"), this);
    mainLayout->addWidget(m_startBtn);

    m_stopBtn = new QPushButton(QStringLiteral("Stop Navigation"), this);
    m_stopBtn->setEnabled(false);
    mainLayout->addWidget(m_stopBtn);

    m_positionLabel = new QLabel(QStringLiteral("Position: -"), this);
    mainLayout->addWidget(m_positionLabel);

    mainLayout->addStretch();

    connect(m_startBtn, &QPushButton::clicked, this, [this]() {
        emit startNavigationRequested();
        m_startBtn->setEnabled(false);
        m_stopBtn->setEnabled(true);
    });

    connect(m_stopBtn, &QPushButton::clicked, this, [this]() {
        emit stopNavigationRequested();
        m_stopBtn->setEnabled(false);
        m_startBtn->setEnabled(true);
    });
}

void NavigationPage::setNavigationStatus(const QString& status)
{
    m_statusLabel->setText(QStringLiteral("Status: %1").arg(status));
}

void NavigationPage::setCurrentPosition(double x, double y, double z)
{
    m_positionLabel->setText(
        QStringLiteral("Position: (%1, %2, %3)")
            .arg(x, 0, 'f', 2)
            .arg(y, 0, 'f', 2)
            .arg(z, 0, 'f', 2));
}
