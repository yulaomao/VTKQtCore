#pragma once

#include <QObject>
#include <QMap>
#include <QString>
#include <QStringList>

class QApplication;

class AppStyleManager : public QObject
{
    Q_OBJECT

public:
    explicit AppStyleManager(QApplication* application, QObject* parent = nullptr);

    void registerStyle(const QString& styleId, const QString& styleSheetResourcePath);
    bool applyStyle(const QString& styleId);

    QString currentStyleId() const;
    QStringList availableStyles() const;

private:
    QString loadStyleSheet(const QString& styleSheetResourcePath) const;

    QApplication* m_application = nullptr;
    QMap<QString, QString> m_styleResources;
    QString m_currentStyleId;
};