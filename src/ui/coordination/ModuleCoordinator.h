#pragma once

#include <QObject>
#include <QWidget>
#include <QVector>
#include <QString>
#include <QVariantMap>

#include "contracts/UiAction.h"
#include "contracts/LogicNotification.h"

class ILogicGateway;
class UiActionDispatcher;

class ModuleCoordinator : public QObject
{
    Q_OBJECT

public:
    enum class AuxiliaryRegion {
        Right,
        Bottom
    };

    ModuleCoordinator(const QString& moduleId, ILogicGateway* gateway,
                      QObject* parent = nullptr);
    ~ModuleCoordinator() override = default;

    QString getModuleId() const;
    void setMainPage(QWidget* page);
    QWidget* getMainPage() const;
    UiActionDispatcher* getActionDispatcher() const;
    void addAuxiliaryWidget(QWidget* widget,
                            AuxiliaryRegion region = AuxiliaryRegion::Right);
    QVector<QWidget*> getAuxiliaryWidgets(AuxiliaryRegion region) const;

    void activate();
    void deactivate();

    void sendModuleAction(UiAction::ActionType type, const QVariantMap& payload = {});

public slots:
    void onModuleNotification(const LogicNotification& notification);

signals:
    void activated();
    void deactivated();
    void moduleAction(const UiAction& action);
    void notificationForPage(const LogicNotification& notification);

private:
    QString m_moduleId;
    ILogicGateway* m_gateway;
    UiActionDispatcher* m_actionDispatcher;
    QWidget* m_mainPage;
    QVector<QWidget*> m_rightAuxiliaryWidgets;
    QVector<QWidget*> m_bottomAuxiliaryWidgets;
};
