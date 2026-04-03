#include "InterModuleSenderWidget.h"

#include "InterModuleTestConstants.h"
#include "ui/coordination/UiActionDispatcher.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSizePolicy>

namespace {

QString senderWidgetStyle()
{
    return QString::fromLatin1(R"(
QFrame#interModuleSenderWidget {
    background-color: #ffffff;
    border: 1px solid #d8e3ea;
    border-radius: 12px;
}

QLabel#interModuleSenderTitle {
    color: #16334c;
    font-size: 12px;
    font-weight: 700;
}

QLineEdit#interModuleSenderInput {
    min-height: 30px;
    min-width: 240px;
    padding: 0 10px;
    border: 1px solid #c9d7e3;
    border-radius: 8px;
    background-color: #f7fbfd;
    color: #17364e;
}

QPushButton#interModuleSenderButton {
    min-height: 30px;
    padding: 0 14px;
    font-weight: 600;
}
)");
}

}

InterModuleSenderWidget::InterModuleSenderWidget(UiActionDispatcher* dispatcher, QWidget* parent)
    : QFrame(parent)
    , m_actionDispatcher(dispatcher)
{
    setObjectName(QStringLiteral("interModuleSenderWidget"));
    setStyleSheet(senderWidgetStyle());
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);
    layout->setSpacing(8);

    auto* titleLabel = new QLabel(QStringLiteral("模块 A"), this);
    titleLabel->setObjectName(QStringLiteral("interModuleSenderTitle"));
    layout->addWidget(titleLabel);

    m_input = new QLineEdit(this);
    m_input->setObjectName(QStringLiteral("interModuleSenderInput"));
    m_input->setPlaceholderText(QStringLiteral("输入发送给模块 B 的文本"));
    layout->addWidget(m_input);

    m_sendButton = new QPushButton(QStringLiteral("Logic 发送"), this);
    m_sendButton->setObjectName(QStringLiteral("interModuleSenderButton"));
    layout->addWidget(m_sendButton);

    connect(m_sendButton, &QPushButton::clicked,
            this, &InterModuleSenderWidget::submitText);
    connect(m_input, &QLineEdit::returnPressed,
            this, &InterModuleSenderWidget::submitText);
}

void InterModuleSenderWidget::submitText()
{
    if (!m_actionDispatcher || !m_input) {
        return;
    }

    const QString text = m_input->text().trimmed();
    if (text.isEmpty()) {
        m_input->setFocus();
        return;
    }

    m_actionDispatcher->sendCommand(
        InterModuleTest::sendTextCommand(),
        {{QStringLiteral("text"), text}});
    m_input->selectAll();
}