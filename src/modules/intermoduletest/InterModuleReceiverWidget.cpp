#include "InterModuleReceiverWidget.h"

#include "InterModuleTestConstants.h"
#include "logic/gateway/ILogicGateway.h"
#include "ui/coordination/ModuleUiEventBinding.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>
#include <QSizePolicy>

namespace {

QString receiverWidgetStyle()
{
    return QString::fromLatin1(R"(
QFrame#interModuleReceiverWidget {
    background-color: #ffffff;
    border: 1px solid #d8e3ea;
    border-radius: 12px;
}

QLabel#interModuleReceiverTitle {
    color: #16334c;
    font-size: 12px;
    font-weight: 700;
}

QLabel#interModuleReceiverHint {
    color: #667f93;
    font-size: 12px;
}

QLabel#interModuleReceiverValue {
    color: #17364e;
    font-size: 12px;
    font-weight: 600;
    background-color: #f7fbfd;
    border: 1px solid #dbe5ec;
    border-radius: 8px;
    padding: 6px 10px;
}

QLabel#interModuleReceiverPreviewValue {
    color: #8a4b12;
    font-size: 12px;
    font-weight: 600;
    background-color: #fff4e8;
    border: 1px solid #f0d5bb;
    border-radius: 8px;
    padding: 6px 10px;
}
)");
}

bool isReceiverTargeted(const LogicNotification& notification)
{
    return notification.targetScope == LogicNotification::AllModules ||
           (notification.targetScope == LogicNotification::ModuleList &&
            notification.targetModules.contains(InterModuleTest::receiverModuleId()));
}

}

InterModuleReceiverWidget::InterModuleReceiverWidget(ILogicGateway* gateway, QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("interModuleReceiverWidget"));
    setStyleSheet(receiverWidgetStyle());
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    auto* titleLabel = new QLabel(QStringLiteral("模块 B"), this);
    titleLabel->setObjectName(QStringLiteral("interModuleReceiverTitle"));
    layout->addWidget(titleLabel);

    auto* contentLayout = new QVBoxLayout();
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(6);

    auto* previewHintLabel = new QLabel(QStringLiteral("UI 事件预览:"), this);
    previewHintLabel->setObjectName(QStringLiteral("interModuleReceiverHint"));
    contentLayout->addWidget(previewHintLabel);

    m_previewLabel = new QLabel(QStringLiteral("等待模块 A 触发 UI 事件"), this);
    m_previewLabel->setObjectName(QStringLiteral("interModuleReceiverPreviewValue"));
    m_previewLabel->setMinimumWidth(260);
    contentLayout->addWidget(m_previewLabel);

    auto* messageHintLabel = new QLabel(QStringLiteral("Logic 持久文本:"), this);
    messageHintLabel->setObjectName(QStringLiteral("interModuleReceiverHint"));
    contentLayout->addWidget(messageHintLabel);

    m_messageLabel = new QLabel(QStringLiteral("等待模块 A 发送文本"), this);
    m_messageLabel->setObjectName(QStringLiteral("interModuleReceiverValue"));
    m_messageLabel->setMinimumWidth(260);
    contentLayout->addWidget(m_messageLabel);

    layout->addLayout(contentLayout);

    if (gateway) {
        ModuleUiEventBinding::bind(
            gateway,
            InterModuleTest::receiverModuleId(),
            InterModuleTest::previewTextEvent(),
            this,
            [this](const QVariantMap& payload) {
                setPreviewText(payload.value(QStringLiteral("text")).toString());
            });
        connect(gateway, &ILogicGateway::notificationReceived,
                this, &InterModuleReceiverWidget::onGatewayNotification);
    }
}

void InterModuleReceiverWidget::setPreviewText(const QString& text)
{
    if (!m_previewLabel || text.trimmed().isEmpty()) {
        return;
    }

    m_previewLabel->setText(text);
}

void InterModuleReceiverWidget::setCommittedText(const QString& text)
{
    if (!m_messageLabel || text.trimmed().isEmpty()) {
        return;
    }

    m_messageLabel->setText(text);
}

void InterModuleReceiverWidget::onGatewayNotification(const LogicNotification& notification)
{
    if (!m_messageLabel || notification.eventType != LogicNotification::CustomEvent) {
        return;
    }
    if (!isReceiverTargeted(notification)) {
        return;
    }
    if (notification.payload.value(QStringLiteral("eventName")).toString() !=
        InterModuleTest::receiverTextUpdatedEvent()) {
        return;
    }

    const QString text = notification.payload.value(QStringLiteral("text")).toString();
    if (!text.isEmpty()) {
        setCommittedText(text);
    }
}