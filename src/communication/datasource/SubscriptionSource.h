#pragma once

#include <QString>
#include <QVariantMap>

#include "SourceBase.h"

class SubscriptionSource : public SourceBase
{
    Q_OBJECT

public:
    SubscriptionSource(const QString& sourceId, const QString& channel,
                       const QString& module, QObject* parent = nullptr);
    ~SubscriptionSource() override = default;

    void start() override;
    void stop() override;
    bool isRunning() const override;

    QString getChannel() const;

public slots:
    void onMessageReceived(const QString& channel, const QVariantMap& data);

private:
    QString m_channel;
    QString m_module;
    bool m_running = false;
};
