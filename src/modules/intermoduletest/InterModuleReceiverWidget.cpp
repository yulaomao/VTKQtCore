#include "InterModuleReceiverWidget.h"

#include "InterModuleTestConstants.h"
#include "logic/gateway/ILogicGateway.h"

#include <QHBoxLayout>
#include <QLabel>
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

    auto* hintLabel = new QLabel(QStringLiteral("收到文本:"), this);
    hintLabel->setObjectName(QStringLiteral("interModuleReceiverHint"));
    layout->addWidget(hintLabel);

    m_messageLabel = new QLabel(QStringLiteral("等待模块 A 发送文本"), this);
    m_messageLabel->setObjectName(QStringLiteral("interModuleReceiverValue"));
    m_messageLabel->setMinimumWidth(240);
    layout->addWidget(m_messageLabel);

    if (gateway) {
        connect(gateway, &ILogicGateway::notificationReceived,
                this, &InterModuleReceiverWidget::onGatewayNotification);
    }
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
        m_messageLabel->setText(text);
    }
}