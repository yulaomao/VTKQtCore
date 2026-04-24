#include "communication/redis/RedisPollingWorker.h"

#include <QCoreApplication>
#include <QTextStream>

namespace {

bool expect(bool condition, const QString& message, QTextStream& err)
{
    if (condition) {
        return true;
    }

    err << "[redis_batch_read_smoke] " << message << Qt::endl;
    return false;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);
    QTextStream err(stderr);

    const QStringList keys {
        QStringLiteral("state.navigation"),
        QStringLiteral("demo:navigation:transform"),
    };

    RedisPollingWorker worker(QStringLiteral("127.0.0.1"), 6379);
    worker.selectDb(1);
    worker.setPollingKeys(keys);

    QVariantMap rawValues;
    QObject::connect(&worker, &RedisPollingWorker::keyValuesReceived,
                     &app, [&rawValues](const QVariantMap& values) {
                         rawValues = values;
                     });

    worker.poll();

    if (!expect(rawValues.size() == keys.size(),
                QStringLiteral("expected %1 raw values, got %2")
                    .arg(keys.size())
                    .arg(rawValues.size()),
                err)) {
        return 1;
    }

    for (const QString& key : keys) {
        if (!expect(rawValues.contains(key),
                    QStringLiteral("missing polled key '%1'").arg(key),
                    err)) {
            return 1;
        }

        if (!expect(rawValues.value(key).isValid(),
                    QStringLiteral("invalid payload for key '%1'").arg(key),
                    err)) {
            return 1;
        }
    }

    const QVariantMap navigationHash = rawValues.value(QStringLiteral("state.navigation")).toMap();
    if (!expect(navigationHash.contains(QStringLiteral("latest")),
                QStringLiteral("state.navigation did not contain latest field"),
                err)) {
        return 1;
    }

    const QVariantMap transformHash = rawValues.value(QStringLiteral("demo:navigation:transform")).toMap();
    if (!expect(transformHash.contains(QStringLiteral("world")),
                QStringLiteral("demo:navigation:transform did not contain world field"),
                err) ||
        !expect(transformHash.contains(QStringLiteral("tip")),
                QStringLiteral("demo:navigation:transform did not contain tip field"),
                err)) {
        return 1;
    }

    out << "[redis_batch_read_smoke] OK: polled " << rawValues.size()
        << " hash keys." << Qt::endl;
    return 0;
}