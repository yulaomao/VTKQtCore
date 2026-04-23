#pragma once

#include <QObject>
#include <QHash>
#include <QPointer>
#include <QString>
#include <QtGlobal>

#include "logic/runtime/IPromptAudioService.h"

class QAudioOutput;
class QMediaPlayer;
class QSoundEffect;
class QUrl;

class PromptAudioService : public QObject, public IPromptAudioService
{
    Q_OBJECT

public:
    explicit PromptAudioService(QObject* parent = nullptr);
    ~PromptAudioService() override;

    bool playPreset(const QString& presetId) override;
    bool playSource(const QString& source) override;
    bool registerPreset(const QString& presetId, const QString& source) override;
    void stopPlayback() override;

private slots:
    void processPendingRequest();

private:
    enum class RequestKind {
        None,
        Stop,
        Preset,
        Source
    };

    struct PlaybackRequest {
        RequestKind kind = RequestKind::None;
        QString presetId;
        QString source;
    };

    void queueRequest(const PlaybackRequest& request);
    void stopCurrentPlayback();
    void startRequest(const PlaybackRequest& request);
    bool tryPlayWithSoundEffect(const QString& source);
    void playWithMediaPlayer(const QString& source);
    QSoundEffect* ensureEffect(const QString& source);
    QString resolvePresetSource(const QString& presetId) const;
    QString normalizePresetId(const QString& presetId) const;
    bool isWaveAudioSource(const QString& source) const;
    QUrl toUrl(const QString& source) const;

    QHash<QString, QString> m_presetSources;
    QHash<QString, QSoundEffect*> m_effectCache;
    QPointer<QSoundEffect> m_activeEffect;
    QMediaPlayer* m_player = nullptr;
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QAudioOutput* m_audioOutput = nullptr;
#endif
    bool m_playQueued = false;
    PlaybackRequest m_pendingRequest;
};