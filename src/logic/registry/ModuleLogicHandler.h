#pragma once

#include <QObject>
#include <QString>

#include "contracts/UiAction.h"
#include "contracts/LogicNotification.h"
#include "communication/datasource/StateSample.h"

class IRedisCommandAccess;
class SceneGraph;

class ModuleLogicHandler : public QObject
{
    Q_OBJECT

public:
    explicit ModuleLogicHandler(const QString& moduleId, QObject* parent = nullptr);

    QString getModuleId() const;
    void setSceneGraph(SceneGraph* scene);
    SceneGraph* getSceneGraph() const;
    void setRedisCommandAccess(IRedisCommandAccess* redisCommandAccess);

    virtual void handleAction(const UiAction& action) = 0;
    virtual void handleStateSample(const StateSample& sample)
    {
        Q_UNUSED(sample);
    }

    virtual void onModuleActivated() {}
    virtual void onModuleDeactivated() {}
    virtual void onResync() {}

signals:
    void logicNotification(const LogicNotification& notification);

protected:
    bool hasRedisCommandAccess() const;
    QVariant readRedisValue(const QString& key);
    QString readRedisStringValue(const QString& key);
    QVariantMap readRedisJsonValue(const QString& key);
    bool writeRedisValue(const QString& key, const QVariant& value);
    bool writeRedisJsonValue(const QString& key, const QVariantMap& value);
    bool publishRedisMessage(const QString& channel, const QByteArray& message);
    bool publishRedisJsonMessage(const QString& channel, const QVariantMap& payload);

private:
    const QString m_moduleId;
    SceneGraph* m_sceneGraph = nullptr;
    IRedisCommandAccess* m_redisCommandAccess = nullptr;
};
