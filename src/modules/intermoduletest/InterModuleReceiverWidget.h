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
    QLabel* m_messageLabel = nullptr;
};