#include "PlanningPage.h"

#include "ui/vtk3d/VtkSceneWindow.h"

#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QWidget>

namespace {

void replaceLayoutWidget(QVBoxLayout* layout, QWidget* newWidget)
{
    if (!layout) {
        return;
    }

    while (layout->count() > 0) {
        QLayoutItem* item = layout->takeAt(0);
        if (QWidget* widget = item->widget()) {
            widget->setParent(nullptr);
            widget->deleteLater();
        }
        delete item;
    }

    if (newWidget) {
        newWidget->setParent(layout->parentWidget());
        layout->addWidget(newWidget);
    }
}

} // namespace

PlanningPage::PlanningPage(QWidget* parent)
    : QWidget(parent)
{
    auto* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(12);

    auto* controlPanel = new QWidget(this);
    controlPanel->setMinimumWidth(260);
    auto* controlLayout = new QVBoxLayout(controlPanel);
    controlLayout->setContentsMargins(0, 0, 0, 0);
    controlLayout->setSpacing(8);

    auto* titleLabel = new QLabel(QStringLiteral("Surgical Planning"), controlPanel);
    controlLayout->addWidget(titleLabel);

    m_statusLabel = new QLabel(QStringLiteral("Plan Status: Not Started"), controlPanel);
    controlLayout->addWidget(m_statusLabel);

    m_generateButton = new QPushButton(QStringLiteral("Generate Plan"), controlPanel);
    controlLayout->addWidget(m_generateButton);

    m_acceptButton = new QPushButton(QStringLiteral("Accept Plan"), controlPanel);
    controlLayout->addWidget(m_acceptButton);

    m_datagenTestButton = new QPushButton(QStringLiteral("Test Datagen: Create Model"), controlPanel);
    controlLayout->addWidget(m_datagenTestButton);

    controlLayout->addStretch();

    auto* scenePanel = new QWidget(this);
    auto* scenePanelLayout = new QVBoxLayout(scenePanel);
    scenePanelLayout->setContentsMargins(0, 0, 0, 0);
    scenePanelLayout->setSpacing(10);

    auto* primaryContainer = new QWidget(scenePanel);
    m_sceneLayout = new QVBoxLayout(primaryContainer);
    m_sceneLayout->setContentsMargins(0, 0, 0, 0);
    m_sceneLayout->setSpacing(0);

    auto* placeholderLabel = new QLabel(QStringLiteral("3D planning window not attached"), primaryContainer);
    placeholderLabel->setAlignment(Qt::AlignCenter);
    m_sceneLayout->addWidget(placeholderLabel);

    auto* overviewTitle = new QLabel(QStringLiteral("Overview"), scenePanel);

    auto* secondaryContainer = new QWidget(scenePanel);
    secondaryContainer->setMinimumHeight(180);
    m_secondarySceneLayout = new QVBoxLayout(secondaryContainer);
    m_secondarySceneLayout->setContentsMargins(0, 0, 0, 0);
    m_secondarySceneLayout->setSpacing(0);

    auto* secondaryPlaceholder = new QLabel(QStringLiteral("Planning overview window not attached"), secondaryContainer);
    secondaryPlaceholder->setAlignment(Qt::AlignCenter);
    m_secondarySceneLayout->addWidget(secondaryPlaceholder);

    scenePanelLayout->addWidget(primaryContainer, 3);
    scenePanelLayout->addWidget(overviewTitle, 0);
    scenePanelLayout->addWidget(secondaryContainer, 2);

    mainLayout->addWidget(controlPanel, 0);
    mainLayout->addWidget(scenePanel, 1);

    connect(m_generateButton, &QPushButton::clicked,
            this, &PlanningPage::generatePlanRequested);
    connect(m_acceptButton, &QPushButton::clicked,
            this, &PlanningPage::acceptPlanRequested);
        connect(m_datagenTestButton, &QPushButton::clicked,
            this, &PlanningPage::datagenModelCreateRequested);
}

void PlanningPage::setSceneWindow(VtkSceneWindow* sceneWindow)
{
    if (!m_sceneLayout || m_sceneWindow == sceneWindow) {
        return;
    }

    m_sceneWindow = sceneWindow;
    replaceLayoutWidget(m_sceneLayout, m_sceneWindow);
}

void PlanningPage::setSecondarySceneWindow(VtkSceneWindow* sceneWindow)
{
    if (!m_secondarySceneLayout || m_secondarySceneWindow == sceneWindow) {
        return;
    }

    m_secondarySceneWindow = sceneWindow;
    replaceLayoutWidget(m_secondarySceneLayout, m_secondarySceneWindow);
}

void PlanningPage::setPlanStatus(const QString& status)
{
    m_statusLabel->setText(QStringLiteral("Plan Status: %1").arg(status));
}
