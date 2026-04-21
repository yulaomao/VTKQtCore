#pragma once

#include <QMap>
#include <QMetaType>
#include <QString>
#include <QStringList>

// Routing entry that maps one Redis key to a target module and a sub-key
// inside the module's per-poll values map.
struct PollingKeyRoute {
    QString module;   // target module ID (e.g. "navigation")
    QString subKey;   // key used inside the module's nested values map
                      // (e.g. "state" or "transform:world")
                      // if empty, the raw Redis key is used as-is
};

class GlobalPollingPlan
{
public:
    explicit GlobalPollingPlan(const QString& planId = QString(),
                               const QStringList& redisKeys = {},
                               int pollIntervalMs = 100);

    QString getPlanId() const;
    void setPlanId(const QString& planId);

    QStringList getRedisKeys() const;
    void setRedisKeys(const QStringList& redisKeys);

    int getPollIntervalMs() const;
    void setPollIntervalMs(int intervalMs);

    bool getChangeDetection() const;
    void setChangeDetection(bool changeDetection);

    double getMaxDispatchRateHz() const;
    void setMaxDispatchRateHz(double rateHz);

    bool isActive() const;
    void setActive(bool active);

    // Per-key routing: maps each Redis key to a module + sub-key.
    // Keys listed here that are not in redisKeys will be ignored.
    void addKeyRoute(const QString& redisKey, const PollingKeyRoute& route);
    void setKeyRoutes(const QMap<QString, PollingKeyRoute>& routes);
    QMap<QString, PollingKeyRoute> getKeyRoutes() const;

    // Redis DB number to SELECT on this connection (0 = default, no SELECT issued)
    int getDb() const;
    void setDb(int db);

private:
    QString m_planId;
    QStringList m_redisKeys;
    int m_pollIntervalMs = 100;
    bool m_changeDetection = true;
    double m_maxDispatchRateHz = 30.0;
    bool m_active = true;
    QMap<QString, PollingKeyRoute> m_keyRoutes;
    int m_db = 0;
};

Q_DECLARE_METATYPE(GlobalPollingPlan)