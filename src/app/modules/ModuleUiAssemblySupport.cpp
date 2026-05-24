#include "ModuleUiAssemblySupport.h"

#include "ModuleUiAssemblyContext.h"
#include "LogicRuntime.h"

#include <QFrame>
#include <QLabel>
#include <QVBoxLayout>

QWidget* createModuleSummaryPanel(const QString& title,
                                  const QString& description,
                                  QLabel** statusLabel,
                                  QWidget* parent)
{
    auto* panel = new QFrame(parent);
    panel->setFrameShape(QFrame::StyledPanel);

    auto* layout = new QVBoxLayout(panel);
    layout->setContentsMargins(12, 12, 12, 12);
    layout->setSpacing(6);

    auto* titleLabel = new QLabel(title, panel);
    titleLabel->setStyleSheet(QStringLiteral("font-weight: 600;"));
    layout->addWidget(titleLabel);

    auto* descriptionLabel = new QLabel(description, panel);
    descriptionLabel->setWordWrap(true);
    layout->addWidget(descriptionLabel);

    *statusLabel = new QLabel(QStringLiteral("等待模块激活"), panel);
    (*statusLabel)->setWordWrap(true);
    layout->addWidget(*statusLabel);

    return panel;
}

bool isModuleUiAssemblyContextValid(const ModuleUiAssemblyContext& context)
{
    return context.applicationCoordinator && context.runtimePort;
}

SceneGraph* sceneGraphFromContext(const ModuleUiAssemblyContext& context)
{
    return context.runtime ? context.runtime->getSceneGraph() : nullptr;
}