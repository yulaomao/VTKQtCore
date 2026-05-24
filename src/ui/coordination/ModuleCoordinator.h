#pragma once

#include <QObject>
#include <QWidget>
#include <QVector>
#include <QString>
#include <QVariantMap>

#include "contracts/UiAction.h"
#include "contracts/LogicNotification.h"

class ILogicRuntimePort;
class UiActionDispatcher;

class ModuleCoordinator : public QObject
{
    Q_OBJECT

public:
    ModuleCoordinator(const QString& moduleId, ILogicRuntimePort* runtimePort,
                      QObject* parent = nullptr);
    ~ModuleCoordinator() override = default;

    QString getModuleId() const;
    void setMainPage(QWidget* page);
    QWidget* getMainPage() const;
    UiActionDispatcher* getActionDispatcher() const;
    void addSupplementaryView(QWidget* widget);
    QVector<QWidget*> getSupplementaryViews() const;

    void activate();
    void deactivate();

public slots:
    void onModuleNotification(const LogicNotification& notification);

signals:
    void activated();
    void deactivated();
    void moduleAction(const UiAction& action);
    void notificationForPage(const LogicNotification& notification);

private:
    QString m_moduleId;
    UiActionDispatcher* m_actionDispatcher;
    QWidget* m_mainPage;
    QVector<QWidget*> m_supplementaryViews;
};
