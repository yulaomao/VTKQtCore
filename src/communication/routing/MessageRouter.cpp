#include "MessageRouter.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>

MessageRouter::MessageRouter(QObject* parent)
    : QObject(parent)
{
}

void MessageRouter::routeIncomingMessage(const QByteArray& rawMessage)
{
    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(rawMessage, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "MessageRouter: invalid JSON message:" << parseError.errorString();
        return;
    }

    QJsonObject obj = doc.object();
    QString category = obj.value(QStringLiteral("category")).toString();
    QString msgId = obj.value(QStringLiteral("msgId")).toString();
    QString module = obj.value(QStringLiteral("module")).toString();
    QVariantMap payload = obj.toVariantMap();

    if (!msgId.isEmpty()) {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (m_deduplicationWindow.contains(msgId)) {
            qint64 lastSeen = m_deduplicationWindow.value(msgId);
            if (now - lastSeen < m_deduplicationTimeoutMs) {
                return;
            }
        }
        m_deduplicationWindow[msgId] = now;
    }

    if (category == QStringLiteral("ActionRequest")) {
        emit actionRequestRouted(module, payload);
    } else if (category == QStringLiteral("Heartbeat")) {
        emit heartbeatReceived();
    } else if (category == QStringLiteral("ResyncRequest")) {
        emit resyncRequestReceived(payload);
    } else if (category == QStringLiteral("ResyncResponse")) {
        emit resyncResponseReceived(payload);
    } else if (category == QStringLiteral("ServerCommand")) {
        QString commandType = obj.value(QStringLiteral("commandType")).toString();
        emit serverCommandRouted(commandType, payload);
    } else {
        qDebug() << "MessageRouter: unrecognized category:" << category;
    }
}

void MessageRouter::clearExpiredDedup()
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    auto it = m_deduplicationWindow.begin();
    while (it != m_deduplicationWindow.end()) {
        if (now - it.value() >= m_deduplicationTimeoutMs) {
            it = m_deduplicationWindow.erase(it);
        } else {
            ++it;
        }
    }
}
