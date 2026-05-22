#pragma once

#include <QFrame>

#include "contracts/LogicNotification.h"

class LogicRuntime;
class QLabel;

class InterModuleReceiverWidget : public QFrame
{
    Q_OBJECT

public:
    explicit InterModuleReceiverWidget(LogicRuntime* runtime, QWidget* parent = nullptr);

public slots:
    void onLogicNotification(const LogicNotification& notification);

private:
    void setPreviewText(const QString& text);
    void setCommittedText(const QString& text);

    QLabel* m_previewLabel = nullptr;
    QLabel* m_messageLabel = nullptr;
};