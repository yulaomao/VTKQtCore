#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "contracts/LogicNotification.h"

class QLabel;
class QTimer;
class QVBoxLayout;
class QPushButton;

class ModuleNavigationModule : public QWidget
{
    Q_OBJECT

public:
    explicit ModuleNavigationModule(QWidget* parent = nullptr);

    void setModuleDisplayOrder(const QStringList& modules);

public slots:
    void setCurrentModule(const QString& moduleId);
    void setConnectionState(const QString& state);
    void onGatewayNotification(const LogicNotification& notification);

signals:
    void moduleSelected(const QString& moduleId);
    void resyncRequested(const QString& reason);

private:
    void rebuildButtons();
    void refreshButtonState();
    void refreshModuleSummary();
    void refreshConnectionBadge();
    void refreshTransformState();
    static QString formatModuleLabel(const QString& moduleId);
    static QString defaultTransformLabel(const QString& nodeId);
    static void repolishWidget(QWidget* widget);

    QStringList m_modules;
    QString m_currentModule;
    QString m_connectionState = QStringLiteral("Disconnected");
    QLabel* m_summaryLabel = nullptr;
    QLabel* m_connectionBadge = nullptr;
    QLabel* m_shellStateLabel = nullptr;
    QVBoxLayout* m_buttonLayout = nullptr;
    QPushButton* m_resyncButton = nullptr;
    QTimer* m_transformRefreshTimer = nullptr;
    QMap<QString, QPushButton*> m_buttons;
    QMap<QString, QLabel*> m_transformIndicators;
    QMap<QString, QLabel*> m_transformLabels;
    QMap<QString, qint64> m_lastTransformUpdateMs;
};
