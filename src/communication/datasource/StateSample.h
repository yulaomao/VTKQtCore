#pragma once

#include <QString>
#include <QUuid>
#include <QVariantMap>
#include <QDateTime>
#include <QMetaType>

struct StateSample {
    QString sampleId;
    QString sourceId;
    QString module;
    QString sampleType;
    qint64 timestampMs = 0;
    QVariantMap data;

    static StateSample create(const QString& sourceId, const QString& module,
                              const QString& type, const QVariantMap& data)
    {
        StateSample s;
        s.sampleId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        s.sourceId = sourceId;
        s.module = module;
        s.sampleType = type;
        s.timestampMs = QDateTime::currentMSecsSinceEpoch();
        s.data = data;
        return s;
    }
};

Q_DECLARE_METATYPE(StateSample)
