#include "NavigationPage.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

NavigationPage::NavigationPage(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    auto* controlPanel = new QWidget(this);
    controlPanel->setMinimumWidth(240);
    auto* controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->setSpacing(8);

    auto* titleLabel = new QLabel(QStringLiteral("Surgical Navigation"), controlPanel);
    controlLayout->addWidget(titleLabel);

    m_statusLabel = new QLabel(QStringLiteral("Status: Idle"), controlPanel);
    controlLayout->addWidget(m_statusLabel);

    m_startBtn = new QPushButton(QStringLiteral("Start Navigation"), controlPanel);
    controlLayout->addWidget(m_startBtn);

    m_stopBtn = new QPushButton(QStringLiteral("Stop Navigation"), controlPanel);
    m_stopBtn->setEnabled(false);
    controlLayout->addWidget(m_stopBtn);

    m_datagenTestBtn = new QPushButton(QStringLiteral("Test Datagen: Create Transform"), controlPanel);
    controlLayout->addWidget(m_datagenTestBtn);

    m_positionLabel = new QLabel(QStringLiteral("Position: -"), controlPanel);
    controlLayout->addWidget(m_positionLabel);

    controlLayout->addStretch();

    auto* scenePanel = new QWidget(this);
    m_sceneLayout = new QVBoxLayout(scenePanel);
    m_sceneLayout->setContentsMargins(0, 0, 0, 0);
    m_sceneLayout->setSpacing(0);

    auto* placeholderLabel = new QLabel(QStringLiteral("3D navigation window not attached"), scenePanel);
    placeholderLabel->setAlignment(Qt::AlignCenter);
    m_sceneLayout->addWidget(placeholderLabel);

    mainLayout->addWidget(controlPanel, 0);
    mainLayout->addWidget(scenePanel, 1);

    connect(m_startBtn, &QPushButton::clicked,
            this, &NavigationPage::startNavigationRequested);

    connect(m_stopBtn, &QPushButton::clicked,
            this, &NavigationPage::stopNavigationRequested);
        connect(m_datagenTestBtn, &QPushButton::clicked,
            this, &NavigationPage::datagenTransformCreateRequested);
}

void NavigationPage::setSceneWindow(QWidget* sceneWindow)
{
    if (!m_sceneLayout || m_sceneWindow == sceneWindow) {
        return;
    }

    while (m_sceneLayout->count() > 0) {
        QLayoutItem* item = m_sceneLayout->takeAt(0);
        if (item->widget()) {
            item->widget()->setParent(nullptr);
            item->widget()->deleteLater();
        }
        delete item;
    }

    m_sceneWindow = sceneWindow;
    if (m_sceneWindow) {
        m_sceneWindow->setParent(m_sceneLayout->parentWidget());
        m_sceneLayout->addWidget(m_sceneWindow);
    }
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

void NavigationPage::setNavigating(bool navigating)
{
    m_startBtn->setEnabled(!navigating);
    m_stopBtn->setEnabled(navigating);
}
