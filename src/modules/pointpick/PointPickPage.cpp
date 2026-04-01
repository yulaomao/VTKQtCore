#include "PointPickPage.h"

#include <QVBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>

PointPickPage::PointPickPage(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* titleLabel = new QLabel(QStringLiteral("Point Selection"), this);
    mainLayout->addWidget(titleLabel);

    m_pointCountLabel = new QLabel(QStringLiteral("Points: 0"), this);
    mainLayout->addWidget(m_pointCountLabel);

    m_pointList = new QListWidget(this);
    mainLayout->addWidget(m_pointList);

    m_confirmButton = new QPushButton(QStringLiteral("Confirm Points"), this);
    mainLayout->addWidget(m_confirmButton);

    mainLayout->addStretch();

    connect(m_confirmButton, &QPushButton::clicked,
            this, &PointPickPage::confirmPointsRequested);
}

void PointPickPage::updatePointList(const QStringList& points)
{
    m_pointList->clear();
    m_pointList->addItems(points);
    m_pointCountLabel->setText(QStringLiteral("Points: %1").arg(points.size()));
}

void PointPickPage::setPointCount(int count)
{
    m_pointCountLabel->setText(QStringLiteral("Points: %1").arg(count));
}
