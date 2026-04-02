#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

class ActiveModuleState : public QObject
{
    Q_OBJECT

public:
    explicit ActiveModuleState(QObject* parent = nullptr);

    void setInitialModule(const QString& module);
    QString getInitialModule() const;

    void setCurrentModule(const QString& module);
    QString getCurrentModule() const;
    QVariantMap createSnapshot() const;

signals:
    void currentModuleChanged(const QString& newModule, const QString& oldModule);

private:
    QString m_currentModule;
    QString m_initialModule;
};
