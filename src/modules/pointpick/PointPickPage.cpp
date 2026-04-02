#include "PointPickPage.h"

#include "ui_PointPickPage.h"

#include <QPushButton>

PointPickPage::PointPickPage(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::PointPickPage)
{
    m_ui->setupUi(this);
    setPointCount(0);
    setConfirmed(false);

    connect(m_ui->confirmButton, &QPushButton::clicked,
            this, &PointPickPage::confirmPointsRequested);

        auto* datagenTestButton = new QPushButton(QStringLiteral("Test Datagen: Create Line"), this);
        m_ui->verticalLayout->insertWidget(m_ui->verticalLayout->count() - 1, datagenTestButton);
        connect(datagenTestButton, &QPushButton::clicked,
            this, &PointPickPage::datagenLineCreateRequested);
}

PointPickPage::~PointPickPage()
{
    delete m_ui;
}

void PointPickPage::updatePointList(const QStringList& points)
{
    m_ui->pointList->clear();
    m_ui->pointList->addItems(points);
    setPointCount(points.size());
}

void PointPickPage::setPointCount(int count)
{
    m_ui->pointCountLabel->setText(QStringLiteral("Points: %1").arg(count));
}

void PointPickPage::setConfirmed(bool confirmed)
{
    m_ui->statusLabel->setText(confirmed
        ? QStringLiteral("Status: Points confirmed")
        : QStringLiteral("Status: Waiting for confirmation"));
    m_ui->confirmButton->setEnabled(!confirmed);
    m_ui->confirmButton->setText(confirmed
        ? QStringLiteral("Points Confirmed")
        : QStringLiteral("Confirm Points"));
}
