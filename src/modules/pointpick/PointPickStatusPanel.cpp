#include "PointPickStatusPanel.h"

#include "ui_PointPickStatusPanel.h"

PointPickStatusPanel::PointPickStatusPanel(QWidget* parent)
    : QWidget(parent)
    , m_ui(new Ui::PointPickStatusPanel)
{
    m_ui->setupUi(this);
    setPointCount(0);
    setConfirmed(false);
}

PointPickStatusPanel::~PointPickStatusPanel()
{
    delete m_ui;
}

void PointPickStatusPanel::setPointCount(int count)
{
    m_ui->pointCountValue->setText(QString::number(count));
}

void PointPickStatusPanel::setConfirmed(bool confirmed)
{
    if (confirmed) {
        m_ui->statusValue->setText(QStringLiteral("Confirmed"));
        m_ui->statusValue->setStyleSheet(
            QStringLiteral("border-radius: 10px; padding: 4px 10px;"
                           "background: #d9f3df; color: #1f6b3a; font-weight: 600;"));
        m_ui->hintLabel->setText(QStringLiteral("Selection is locked. Continue to planning when ready."));
        return;
    }

    m_ui->statusValue->setText(QStringLiteral("Pending"));
    m_ui->statusValue->setStyleSheet(
        QStringLiteral("border-radius: 10px; padding: 4px 10px;"
                       "background: #fff0d9; color: #8f5200; font-weight: 600;"));
    m_ui->hintLabel->setText(QStringLiteral("Review the sampled points and confirm them from the main page."));
}