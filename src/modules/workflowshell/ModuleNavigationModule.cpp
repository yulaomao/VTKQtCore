#include "ModuleNavigationModule.h"

#include <QDateTime>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTimer>
#include <QVBoxLayout>

namespace {

const QStringList kTransformOrder = {
    QStringLiteral("navigation-world-transform"),
    QStringLiteral("navigation-reference-transform"),
    QStringLiteral("navigation-patient-transform"),
    QStringLiteral("navigation-instrument-transform"),
    QStringLiteral("navigation-guide-transform"),
    QStringLiteral("navigation-tip-transform"),
};

QFrame* createCard(QWidget* parent)
{
    auto* card = new QFrame(parent);
    card->setObjectName(QStringLiteral("moduleNavigationCard"));
    return card;
}

QLabel* createSectionTitle(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("moduleNavigationSectionTitle"));
    return label;
}

QString moduleNavigationModuleStyle()
{
    return QString::fromLatin1(R"(
QWidget#moduleNavigationModule {
    background: transparent;
}

QFrame#moduleNavigationCard {
    background-color: #ffffff;
    border: 1px solid #d6e2ea;
    border-radius: 16px;
}

QLabel#moduleNavigationEyebrow {
    color: #7d93a6;
    font-size: 11px;
    font-weight: 700;
}

QLabel#moduleNavigationTitle {
    color: #16334c;
    font-size: 18px;
    font-weight: 700;
}

QLabel#moduleNavigationSummary {
    color: #4f6578;
    font-size: 13px;
}

QLabel#moduleNavigationSectionTitle {
    color: #16334c;
    font-size: 13px;
    font-weight: 700;
}

QLabel#moduleNavigationConnectionBadge {
    color: #31506a;
    background-color: #edf3f7;
    border: 1px solid #d3dee6;
    border-radius: 11px;
    padding: 4px 10px;
    font-weight: 600;
}

QLabel#moduleNavigationConnectionBadge[stateLevel="connected"] {
    color: #17603d;
    background-color: #e8f6ee;
    border: 1px solid #b7e3c7;
}

QLabel#moduleNavigationConnectionBadge[stateLevel="degraded"] {
    color: #8a5a12;
    background-color: #fff5dd;
    border: 1px solid #edd7a4;
}

QLabel#moduleNavigationConnectionBadge[stateLevel="disconnected"] {
    color: #8d3c36;
    background-color: #fdeceb;
    border: 1px solid #efc2bd;
}

QFrame#moduleNavigationTransformChip {
    background-color: #f6fafc;
    border: 1px solid #e0e9ef;
    border-radius: 12px;
}

QLabel#moduleNavigationTransformIndicator {
    min-width: 12px;
    max-width: 12px;
    min-height: 12px;
    max-height: 12px;
    border-radius: 6px;
    background-color: #d05f58;
    border: 1px solid #f3c0bb;
}

QLabel#moduleNavigationTransformIndicator[pulseState="active"] {
    background-color: #29a556;
    border: 1px solid #b9e7c6;
}

QLabel#moduleNavigationTransformIndicator[pulseState="inactive"] {
    background-color: #d05f58;
    border: 1px solid #f3c0bb;
}

QLabel#moduleNavigationTransformName {
    color: #27465f;
    font-size: 12px;
    font-weight: 600;
}

QPushButton#moduleNavigationButton {
    min-height: 42px;
    text-align: left;
    padding: 0 14px;
    border-radius: 12px;
    border: 1px solid #d6e2ea;
    background-color: #f8fbfd;
    color: #18344f;
    font-weight: 600;
}

QPushButton#moduleNavigationButton:hover {
    background-color: #eef6fb;
}

QPushButton#moduleNavigationButton[moduleState="current"] {
    background-color: #d9edf8;
    border: 1px solid #8ab9d3;
    color: #10324a;
}

QPushButton#moduleNavigationButton[moduleState="locked"] {
    background-color: #f3f6f8;
    border: 1px solid #e1e8ed;
    color: #8fa1b0;
}

QPushButton#moduleNavigationActionButton {
    min-height: 38px;
    border-radius: 12px;
    font-weight: 600;
}

QLabel#moduleNavigationStateLabel {
    color: #516677;
    font-size: 12px;
    background-color: #f6fafc;
    border: 1px solid #e0e9ef;
    border-radius: 12px;
    padding: 10px 12px;
}
)");
}

} // namespace

ModuleNavigationModule::ModuleNavigationModule(QWidget* parent)
    : QWidget(parent)
    , m_buttonLayout(new QVBoxLayout)
{
    setObjectName(QStringLiteral("moduleNavigationModule"));
    setStyleSheet(moduleNavigationModuleStyle());

    m_transformRefreshTimer = new QTimer(this);
    m_transformRefreshTimer->setInterval(50);
    connect(m_transformRefreshTimer, &QTimer::timeout,
            this, &ModuleNavigationModule::refreshTransformState);
    m_transformRefreshTimer->start();

    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(12);

    auto* headerCard = createCard(this);
    auto* headerLayout = new QVBoxLayout(headerCard);
    headerLayout->setContentsMargins(16, 16, 16, 16);
    headerLayout->setSpacing(10);

    auto* eyebrow = new QLabel(QStringLiteral("SHELL MODULE"), headerCard);
    eyebrow->setObjectName(QStringLiteral("moduleNavigationEyebrow"));
    headerLayout->addWidget(eyebrow);

    auto* title = new QLabel(QStringLiteral("Module Navigator"), headerCard);
    title->setObjectName(QStringLiteral("moduleNavigationTitle"));
    headerLayout->addWidget(title);

    m_summaryLabel = new QLabel(QStringLiteral("等待模块激活"), headerCard);
    m_summaryLabel->setObjectName(QStringLiteral("moduleNavigationSummary"));
    m_summaryLabel->setWordWrap(true);
    headerLayout->addWidget(m_summaryLabel);

    m_connectionBadge = new QLabel(QStringLiteral("Disconnected"), headerCard);
    m_connectionBadge->setObjectName(QStringLiteral("moduleNavigationConnectionBadge"));
    headerLayout->addWidget(m_connectionBadge, 0, Qt::AlignLeft);

    rootLayout->addWidget(headerCard);

    auto* transformCard = createCard(this);
    auto* transformLayout = new QVBoxLayout(transformCard);
    transformLayout->setContentsMargins(16, 16, 16, 16);
    transformLayout->setSpacing(10);
    transformLayout->addWidget(createSectionTitle(QStringLiteral("Transform Stream"), transformCard));

    auto* transformGrid = new QGridLayout;
    transformGrid->setContentsMargins(0, 0, 0, 0);
    transformGrid->setHorizontalSpacing(8);
    transformGrid->setVerticalSpacing(8);

    for (int index = 0; index < kTransformOrder.size(); ++index) {
        const QString& nodeId = kTransformOrder.at(index);
        auto* chip = new QFrame(transformCard);
        chip->setObjectName(QStringLiteral("moduleNavigationTransformChip"));
        auto* chipLayout = new QHBoxLayout(chip);
        chipLayout->setContentsMargins(10, 10, 10, 10);
        chipLayout->setSpacing(8);

        auto* indicator = new QLabel(chip);
        indicator->setObjectName(QStringLiteral("moduleNavigationTransformIndicator"));
        indicator->setProperty("pulseState", QStringLiteral("inactive"));
        indicator->setFixedSize(12, 12);
        chipLayout->addWidget(indicator, 0, Qt::AlignVCenter);

        auto* label = new QLabel(defaultTransformLabel(nodeId), chip);
        label->setObjectName(QStringLiteral("moduleNavigationTransformName"));
        chipLayout->addWidget(label, 1);

        transformGrid->addWidget(chip, index / 2, index % 2);
        m_transformIndicators.insert(nodeId, indicator);
        m_transformLabels.insert(nodeId, label);
    }
    transformLayout->addLayout(transformGrid);
    rootLayout->addWidget(transformCard);

    auto* moduleListCard = createCard(this);
    auto* moduleListLayout = new QVBoxLayout(moduleListCard);
    moduleListLayout->setContentsMargins(16, 16, 16, 16);
    moduleListLayout->setSpacing(10);
    moduleListLayout->addWidget(createSectionTitle(QStringLiteral("Registered Modules"), moduleListCard));

    m_buttonLayout->setContentsMargins(0, 0, 0, 0);
    m_buttonLayout->setSpacing(8);
    moduleListLayout->addLayout(m_buttonLayout);
    rootLayout->addWidget(moduleListCard);

    auto* controlCard = createCard(this);
    auto* controlLayout = new QVBoxLayout(controlCard);
    controlLayout->setContentsMargins(16, 16, 16, 16);
    controlLayout->setSpacing(10);
    controlLayout->addWidget(createSectionTitle(QStringLiteral("Shell Controls"), controlCard));

    m_resyncButton = new QPushButton(QStringLiteral("Resync State"), controlCard);
    m_resyncButton->setObjectName(QStringLiteral("moduleNavigationActionButton"));
    controlLayout->addWidget(m_resyncButton);

    m_shellStateLabel = new QLabel(
        QStringLiteral("框架只维护当前激活模块，不再限制模块顺序或动作准入。"),
        controlCard);
    m_shellStateLabel->setObjectName(QStringLiteral("moduleNavigationStateLabel"));
    m_shellStateLabel->setWordWrap(true);
    controlLayout->addWidget(m_shellStateLabel);

    rootLayout->addWidget(controlCard);
    rootLayout->addStretch(1);

    connect(m_resyncButton, &QPushButton::clicked, this, [this]() {
        emit resyncRequested(QStringLiteral("module_navigation_module"));
    });

    refreshConnectionBadge();
    refreshTransformState();
}

void ModuleNavigationModule::setModuleDisplayOrder(const QStringList& modules)
{
    m_modules = modules;
    rebuildButtons();
    refreshButtonState();
}

void ModuleNavigationModule::setCurrentModule(const QString& moduleId)
{
    m_currentModule = moduleId;
    refreshButtonState();
    refreshModuleSummary();
}

void ModuleNavigationModule::setConnectionState(const QString& state)
{
    m_connectionState = state;
    refreshConnectionBadge();
    refreshModuleSummary();
}

void ModuleNavigationModule::onGatewayNotification(const LogicNotification& notification)
{
    if (notification.eventType != LogicNotification::CustomEvent) {
        return;
    }
    if (notification.payload.value(QStringLiteral("eventName")).toString() !=
        QStringLiteral("navigation_transform_health")) {
        return;
    }

    const QVariantList transforms = notification.payload.value(QStringLiteral("transforms")).toList();
    for (const QVariant& item : transforms) {
        const QVariantMap transform = item.toMap();
        const QString nodeId = transform.value(QStringLiteral("nodeId")).toString();
        if (nodeId.isEmpty()) {
            continue;
        }

        const qint64 lastSampleTimestampMs = transform.value(
            QStringLiteral("lastSampleTimestampMs")).toLongLong();
        if (lastSampleTimestampMs > 0) {
            m_lastTransformUpdateMs.insert(nodeId, lastSampleTimestampMs);
        }

        const QString displayName = transform.value(QStringLiteral("displayName")).toString();
        if (!displayName.isEmpty() && m_transformLabels.contains(nodeId)) {
            m_transformLabels.value(nodeId)->setText(displayName);
        }
    }

    refreshTransformState();
}

void ModuleNavigationModule::rebuildButtons()
{
    while (QLayoutItem* item = m_buttonLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
    m_buttons.clear();

    for (int index = 0; index < m_modules.size(); ++index) {
        const QString moduleId = m_modules.at(index);
        auto* button = new QPushButton(
            QStringLiteral("%1  %2")
                .arg(index + 1, 2, 10, QLatin1Char('0'))
                .arg(formatModuleLabel(moduleId)),
            this);
        button->setObjectName(QStringLiteral("moduleNavigationButton"));
        button->setCheckable(true);
        button->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        connect(button, &QPushButton::clicked, this, [this, moduleId]() {
            emit moduleSelected(moduleId);
        });
        m_buttonLayout->addWidget(button);
        m_buttons.insert(moduleId, button);
    }
}

void ModuleNavigationModule::refreshButtonState()
{
    for (auto it = m_buttons.begin(); it != m_buttons.end(); ++it) {
        const bool current = it.key() == m_currentModule;
        it.value()->setEnabled(true);
        it.value()->setChecked(current);
        it.value()->setProperty(
            "moduleState",
            current ? QStringLiteral("current") : QStringLiteral("available"));
        repolishWidget(it.value());
    }
}

void ModuleNavigationModule::refreshModuleSummary()
{
    if (!m_summaryLabel) {
        return;
    }

    QString summary = QStringLiteral("等待模块激活");
    if (!m_currentModule.isEmpty()) {
        summary = QStringLiteral("当前模块: %1")
                      .arg(formatModuleLabel(m_currentModule));
    }

    summary += QStringLiteral("\n已装配模块数: %1").arg(m_modules.size());
    summary += QStringLiteral("\n连接状态：%1").arg(m_connectionState);

    m_summaryLabel->setText(summary);
}

void ModuleNavigationModule::refreshConnectionBadge()
{
    if (!m_connectionBadge) {
        return;
    }

    QString stateLevel = QStringLiteral("disconnected");
    if (m_connectionState.compare(QStringLiteral("Connected"), Qt::CaseInsensitive) == 0) {
        stateLevel = QStringLiteral("connected");
    } else if (m_connectionState.compare(QStringLiteral("Degraded"), Qt::CaseInsensitive) == 0) {
        stateLevel = QStringLiteral("degraded");
    }

    m_connectionBadge->setText(m_connectionState);
    m_connectionBadge->setProperty("stateLevel", stateLevel);
    repolishWidget(m_connectionBadge);
}

void ModuleNavigationModule::refreshTransformState()
{
    const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
    for (auto it = m_transformIndicators.begin(); it != m_transformIndicators.end(); ++it) {
        const qint64 lastSampleTimestampMs = m_lastTransformUpdateMs.value(it.key(), 0);
        const bool active = lastSampleTimestampMs > 0 &&
            (nowMs - lastSampleTimestampMs) <= 100;
        it.value()->setProperty(
            "pulseState",
            active ? QStringLiteral("active") : QStringLiteral("inactive"));
        repolishWidget(it.value());
    }
}

QString ModuleNavigationModule::formatModuleLabel(const QString& moduleId)
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

QString ModuleNavigationModule::defaultTransformLabel(const QString& nodeId)
{
    if (nodeId == QStringLiteral("navigation-world-transform")) {
        return QStringLiteral("World");
    }
    if (nodeId == QStringLiteral("navigation-reference-transform")) {
        return QStringLiteral("Reference");
    }
    if (nodeId == QStringLiteral("navigation-patient-transform")) {
        return QStringLiteral("Patient");
    }
    if (nodeId == QStringLiteral("navigation-instrument-transform")) {
        return QStringLiteral("Instrument");
    }
    if (nodeId == QStringLiteral("navigation-guide-transform")) {
        return QStringLiteral("Guide");
    }
    if (nodeId == QStringLiteral("navigation-tip-transform")) {
        return QStringLiteral("Tip");
    }
    return nodeId;
}

void ModuleNavigationModule::repolishWidget(QWidget* widget)
{
    if (!widget || !widget->style()) {
        return;
    }
    widget->style()->unpolish(widget);
    widget->style()->polish(widget);
    widget->update();
}
