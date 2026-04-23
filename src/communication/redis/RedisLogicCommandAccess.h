#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include <mutex>

#include "communication/hub/IRedisCommandAccess.h"

struct redisContext;

class RedisLogicCommandAccess : public QObject, public IRedisCommandAccess
{
    Q_OBJECT

public:
    explicit RedisLogicCommandAccess(QObject* parent = nullptr);
    ~RedisLogicCommandAccess() override;

    void connectToServer(const QString& host, int port, int db = 0);
    void disconnect();

    bool isAvailable() const override;
    QVariant readValue(const QString& key) override;
    QString readStringValue(const QString& key) override;
    QVariantMap readJsonValue(const QString& key) override;
    QVariant readHashValue(const QString& hashKey, const QString& field) override;
    QString readHashStringValue(const QString& hashKey, const QString& field) override;
    QVariantMap readHashJsonValue(const QString& hashKey, const QString& field) override;
    QVariant readHashValue(const QStringList& path) override;
    QString readHashStringValue(const QStringList& path) override;
    QVariantMap readHashJsonValue(const QStringList& path) override;
    bool writeValue(const QString& key, const QVariant& value) override;
    bool writeJsonValue(const QString& key, const QVariantMap& value) override;
    bool writeHashValue(const QStringList& path, const QVariant& value) override;
    bool writeHashJsonValue(const QStringList& path, const QVariantMap& value) override;
    bool publishMessage(const QString& channel, const QByteArray& message) override;
    bool publishJsonMessage(const QString& channel, const QVariantMap& payload) override;

signals:
    void errorOccurred(const QString& errorMessage);

private:
    redisContext* createContext(QString* errorMessage) const;
    void closeContextLocked();
    bool ensureConnectedLocked(QString* errorMessage);

    QString m_host;
    int m_port = 0;
    int m_db = 0;
    int m_connectTimeoutMs = 2000;
    mutable std::mutex m_mutex;
    redisContext* m_context = nullptr;
};