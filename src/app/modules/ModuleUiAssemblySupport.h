#pragma once

#include <QString>

class QLabel;
class QWidget;
class SceneGraph;
struct ModuleUiAssemblyContext;

QWidget* createModuleSummaryPanel(const QString& title,
                                  const QString& description,
                                  QLabel** statusLabel,
                                  QWidget* parent);
bool isModuleUiAssemblyContextValid(const ModuleUiAssemblyContext& context);
SceneGraph* sceneGraphFromContext(const ModuleUiAssemblyContext& context);