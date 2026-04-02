#include "ModuleStatusBarModule.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>

namespace {

QString moduleStatusBarStyle()
{
    return QString::fromLatin1(R"(
QWidget#moduleStatusBarModule {
    background-color: #ffffff;
    border: 1px solid #d8e3ea;
    border-radius: 14px;
}

QPushButton#moduleStatusBarAction {
    min-height: 34px;
    padding: 0 14px;
    border-radius: 10px;
    border: 1px solid #cfdce5;
    background-color: #f7fbfd;
    color: #17364e;
    font-weight: 600;
}

QPushButton#moduleStatusBarAction:hover {
    background-color: #ebf4f9;
}

QLabel#moduleStatusBarStep {
    color: #16334c;
    font-size: 13px;
    font-weight: 700;
}

QLabel#moduleStatusBarMeta {
    color: #52687a;
    font-size: 12px;
}
)");
}

} // namespace

ModuleStatusBarModule::ModuleStatusBarModule(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("moduleStatusBarModule"));
    setStyleSheet(moduleStatusBarStyle());

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(14, 10, 14, 10);
    layout->setSpacing(8);

    auto* resyncButton = new QPushButton(QStringLiteral("Resync"), this);
    resyncButton->setObjectName(QStringLiteral("moduleStatusBarAction"));

    m_stepLabel = new QLabel(QStringLiteral("No active module"), this);
    m_stepLabel->setObjectName(QStringLiteral("moduleStatusBarStep"));
    m_connectionLabel = new QLabel(QStringLiteral("Connection: Disconnected"), this);
    m_connectionLabel->setObjectName(QStringLiteral("moduleStatusBarMeta"));
    m_healthLabel = new QLabel(QStringLiteral("Health: offline"), this);
    m_healthLabel->setObjectName(QStringLiteral("moduleStatusBarMeta"));

    layout->addWidget(m_stepLabel, 1);
    layout->addWidget(m_connectionLabel);
    layout->addWidget(m_healthLabel);
    layout->addWidget(resyncButton);

    connect(resyncButton, &QPushButton::clicked, this, [this]() {
        emit resyncRequested(QStringLiteral("module_status_bar_module"));
    });
}

void ModuleStatusBarModule::setModuleDisplayOrder(const QStringList& modules)
{
    m_modules = modules;
    refreshState();
}

void ModuleStatusBarModule::setCurrentModule(const QString& moduleId)
{
    m_currentModule = moduleId;
    refreshState();
}

void ModuleStatusBarModule::setConnectionState(const QString& state)
{
    m_connectionState = state;
    refreshState();
}

void ModuleStatusBarModule::setHealthSnapshot(const QVariantMap& snapshot)
{
    const QString healthState = snapshot.value(QStringLiteral("healthState")).toString();
    if (!healthState.isEmpty()) {
        m_healthState = healthState;
    }
    refreshState();
}

void ModuleStatusBarModule::refreshState()
{
    const QString stepText = m_currentModule.isEmpty()
        ? QStringLiteral("No active module")
        : QStringLiteral("Active Module: %1").arg(formatModuleLabel(m_currentModule));

    m_stepLabel->setText(stepText);
    m_connectionLabel->setText(QStringLiteral("Connection: %1").arg(m_connectionState));
    m_healthLabel->setText(
        QStringLiteral("Health: %1 | Modules: %2").arg(m_healthState).arg(m_modules.size()));
}

QString ModuleStatusBarModule::formatModuleLabel(const QString& moduleId)
{
    if (moduleId == QStringLiteral("datagen")) {
        return QStringLiteral("Data Generator");
    }
    if (moduleId == QStringLiteral("params")) {
        return QStringLiteral("Parameters");
    }
    if (moduleId == QStringLiteral("pointpick")) {
        return QStringLiteral("Point Pick");
    }
    if (moduleId == QStringLiteral("planning")) {
        return QStringLiteral("Planning");
    }
    if (moduleId == QStringLiteral("navigation")) {
        return QStringLiteral("Navigation");
    }
    return moduleId;
}
