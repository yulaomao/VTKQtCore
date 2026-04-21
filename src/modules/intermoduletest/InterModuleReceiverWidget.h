#pragma once

#include <QFrame>

#include "contracts/LogicNotification.h"

class ILogicGateway;
class QLabel;

class InterModuleReceiverWidget : public QFrame
{
    Q_OBJECT

public:
    explicit InterModuleReceiverWidget(ILogicGateway* gateway, QWidget* parent = nullptr);

public slots:
    void onGatewayNotification(const LogicNotification& notification);

private:
    void setPreviewText(const QString& text);
    void setCommittedText(const QString& text);

    QLabel* m_previewLabel = nullptr;
    QLabel* m_messageLabel = nullptr;
};