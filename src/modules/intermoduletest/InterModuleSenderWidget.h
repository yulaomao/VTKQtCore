#pragma once

#include <QFrame>

class QLineEdit;
class QPushButton;
class UiActionDispatcher;

class InterModuleSenderWidget : public QFrame
{
    Q_OBJECT

public:
    explicit InterModuleSenderWidget(UiActionDispatcher* dispatcher, QWidget* parent = nullptr);

private slots:
    void submitText();

private:
    UiActionDispatcher* m_actionDispatcher = nullptr;
    QLineEdit* m_input = nullptr;
    QPushButton* m_sendButton = nullptr;
};