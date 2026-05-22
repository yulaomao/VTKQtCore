#pragma once

#include <QObject>
#include <QString>
#include <QVariantMap>

#include <memory>

#include "communication/datasource/StateSample.h"
#include "contracts/UiAction.h"

class MessageRouter;
class QTimer;

namespace socket_dc {
class SocketClient;
}

class CommunicationHub : public QObject
{
    Q_OBJECT

public:
    explicit CommunicationHub(QObject* parent = nullptr);
    ~CommunicationHub() override;

    void initialize();
    void setServerEndpoint(const QString& host, quint16 port);
    void setOutboundChannels(const QString& controlPublishChannel,
                             const QString& ackChannel);
    void addRoutingChannel(const QString& channel);
    void sendActionRequest(const UiAction& action, bool loopbackToLocal = true);
    void sendResyncRequest(const QString& reason, bool loopbackToLocal = true);
    void sendTargetedMessage(const QString& targetName,
                             const QString& messageType,
                             const QVariantMap& payload = {});
    void start();
    void stop();
    QString getConnectionStateName() const;
    QVariantMap getHealthSnapshot() const;

signals:
    void controlMessageReceived(const QString& module, const QVariantMap& payload);
    void serverCommandReceived(const QString& commandType, const QVariantMap& payload);
    void stateSampleReceived(const StateSample& sample);
    void communicationError(const QString& source, const QString& errorMessage);
    void communicationIssue(const QString& source, const QString& severity,
                            const QString& errorCode, const QString& errorMessage,
                            const QVariantMap& context);
    void connectionStateChanged(const QString& state);
    void healthSnapshotChanged(const QVariantMap& snapshot);
    void heartbeatReceived();

private:
    void wireRouter();
    void configureSocketCallbacks();
    void onSocketConnected();
    void onSocketDisconnected();
    void onSocketError(const QString& errorMessage);
    void onSocketRawMessage(const QByteArray& message);
    void routeEnvelopeMessage(const QVariantMap& envelope, const QByteArray& rawMessage);
    bool sendJson(const QVariantMap& payload);
    bool sendEnvelope(const QString& module, const QString& type, const QVariantMap& value);
    void emitIssue(const QString& source, const QString& severity,
                   const QString& errorCode, const QString& errorMessage,
                   const QVariantMap& context = {});
    void refreshHealthSnapshot();

    std::unique_ptr<socket_dc::SocketClient> m_socketClient;
    MessageRouter* m_messageRouter = nullptr;
    QString m_host = QStringLiteral("127.0.0.1");
    quint16 m_port = 9000;
    QString m_connectionState = QStringLiteral("Disconnected");
    QString m_controlPublishChannel = QStringLiteral("control.upstream");
    QString m_ackChannel = QStringLiteral("control.ack");
    QString m_clientInstanceId;
    QVariantMap m_healthSnapshot;
    bool m_initialized = false;
    bool m_started = false;
    qint64 m_lastHeartbeatMs = 0;
    qint64 m_lastControlMessageMs = 0;
    qint64 m_lastStateSampleMs = 0;
    int m_routingErrorCount = 0;
    int m_transportErrorCount = 0;
    int m_receivedControlCount = 0;
    int m_receivedSampleCount = 0;
    int m_sentMessageCount = 0;
};
