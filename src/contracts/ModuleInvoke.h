#pragma once

#include <QDateTime>
#include <QString>
#include <QUuid>
#include <QVariantMap>

struct ModuleInvokeRequest
{
    QString requestId;
    QString sourceModule;
    QString targetModule;
    QString method;
    qint64 timestampMs = 0;
    QVariantMap payload;

    static ModuleInvokeRequest create(const QString& sourceModule,
                                      const QString& targetModule,
                                      const QString& method,
                                      const QVariantMap& payload = {})
    {
        ModuleInvokeRequest request;
        request.requestId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        request.sourceModule = sourceModule;
        request.targetModule = targetModule;
        request.method = method;
        request.timestampMs = QDateTime::currentMSecsSinceEpoch();
        request.payload = payload;
        return request;
    }
};

struct ModuleInvokeResult
{
    bool ok = false;
    QString errorCode;
    QString message;
    QVariantMap payload;

    static ModuleInvokeResult success(const QVariantMap& payload = {},
                                      const QString& message = QString())
    {
        ModuleInvokeResult result;
        result.ok = true;
        result.payload = payload;
        result.message = message;
        return result;
    }

    static ModuleInvokeResult failure(const QString& errorCode,
                                      const QString& message,
                                      const QVariantMap& payload = {})
    {
        ModuleInvokeResult result;
        result.ok = false;
        result.errorCode = errorCode;
        result.message = message;
        result.payload = payload;
        return result;
    }
};