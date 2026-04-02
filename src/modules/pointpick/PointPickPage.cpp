#include "PointPickPage.h"

#include "PointPickUiCommands.h"
#include "ui/coordination/UiActionDispatcher.h"
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
            this, [this]() {
                if (m_actionDispatcher) {
                    m_actionDispatcher->sendCommand(PointPickUiCommands::confirmPoints());
                }
            });

    auto* datagenTestButton = new QPushButton(QStringLiteral("Test Datagen: Create Line"), this);
    m_ui->verticalLayout->insertWidget(m_ui->verticalLayout->count() - 1, datagenTestButton);
    connect(datagenTestButton, &QPushButton::clicked,
            this, [this]() {
                if (!m_actionDispatcher) {
                    return;
                }

                m_actionDispatcher->sendTargetedCommand(
                    QStringLiteral("datagen"),
                    QStringLiteral("create_node"),
                    {{QStringLiteral("nodeType"), QStringLiteral("line")},
                     {QStringLiteral("name"), QStringLiteral("PointPick Relay Path")},
                     {QStringLiteral("count"), 5},
                     {QStringLiteral("spacing"), 20.0},
                     {QStringLiteral("closed"), false},
                     {QStringLiteral("relaySourceModule"), QStringLiteral("pointpick")}});
            });
}

void PointPickPage::setActionDispatcher(UiActionDispatcher* dispatcher)
{
    m_actionDispatcher = dispatcher;
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
