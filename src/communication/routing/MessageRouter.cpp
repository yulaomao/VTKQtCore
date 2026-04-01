#include "MessageRouter.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QDebug>

MessageRouter::MessageRouter(QObject* parent)
    : QObject(parent)
    , m_cleanupTimer(new QTimer(this))
{
    m_cleanupTimer->setInterval(5000);
    connect(m_cleanupTimer, &QTimer::timeout, this, &MessageRouter::clearExpiredDedup);
    m_cleanupTimer->start();
}

void MessageRouter::routeIncomingMessage(const QByteArray& rawMessage)
{
    clearExpiredDedup();

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(rawMessage, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        qWarning() << "MessageRouter: invalid JSON message:" << parseError.errorString();
        emit routingError(QStringLiteral("Invalid JSON message: %1").arg(parseError.errorString()));
        return;
    }

    QJsonObject obj = doc.object();
    QString category = obj.value(QStringLiteral("category")).toString();
    QString msgId = obj.value(QStringLiteral("msgId")).toString();
    QString module = obj.value(QStringLiteral("module")).toString();
    QVariantMap payload = obj.toVariantMap();

    if (category.isEmpty()) {
        emit routingError(QStringLiteral("Message category is missing"));
        return;
    }

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

    if (category == QStringLiteral("Ack")) {
        emit ackReceived(payload);
    } else if (category == QStringLiteral("ActionRequest")) {
        emit actionRequestRouted(module, payload);
    } else if (category == QStringLiteral("Heartbeat")) {
        emit heartbeatReceived(payload);
    } else if (category == QStringLiteral("ResyncRequest")) {
        emit resyncRequestReceived(payload);
    } else if (category == QStringLiteral("ResyncResponse")) {
        emit resyncResponseReceived(payload);
    } else if (category == QStringLiteral("ServerCommand")) {
        QString commandType = obj.value(QStringLiteral("commandType")).toString();
        emit serverCommandRouted(commandType, payload);
    } else {
        qDebug() << "MessageRouter: unrecognized category:" << category;
        emit routingError(QStringLiteral("Unrecognized message category: %1").arg(category));
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
