#pragma once

#include <QObject>
#include <QWidget>
#include <QVector>
#include <QString>
#include <QVariantMap>

#include "contracts/UiAction.h"
#include "contracts/LogicNotification.h"

class ILogicGateway;

class ModuleCoordinator : public QObject
{
    Q_OBJECT

public:
    ModuleCoordinator(const QString& moduleId, ILogicGateway* gateway,
                      QObject* parent = nullptr);
    ~ModuleCoordinator() override = default;

    QString getModuleId() const;
    void setMainPage(QWidget* page);
    QWidget* getMainPage() const;
    void addAuxiliaryWidget(QWidget* widget);
    QVector<QWidget*> getAuxiliaryWidgets() const;

    void activate();
    void deactivate();

    void sendModuleAction(UiAction::ActionType type, const QVariantMap& payload = {});

public slots:
    void onModuleNotification(const LogicNotification& notification);

signals:
    void moduleAction(const UiAction& action);
    void notificationForPage(const LogicNotification& notification);

private:
    QString m_moduleId;
    ILogicGateway* m_gateway;
    QWidget* m_mainPage;
    QVector<QWidget*> m_auxiliaryWidgets;
};
