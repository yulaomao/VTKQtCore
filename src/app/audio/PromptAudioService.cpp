#include "PromptAudioService.h"

#include <QDebug>
#include <QFile>
#include <QMetaObject>
#include <QUrl>

#if VTKQTCORE_HAS_QT_MULTIMEDIA
#include <QMediaPlayer>
#include <QSoundEffect>
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
#include <QAudioOutput>
#endif
#endif

namespace {

QString normalizeSource(const QString& source)
{
    return source.trimmed();
}

}

PromptAudioService::PromptAudioService(QObject* parent)
    : QObject(parent)
#if VTKQTCORE_HAS_QT_MULTIMEDIA
    , m_player(new QMediaPlayer(this))
#endif
{
#if VTKQTCORE_HAS_QT_MULTIMEDIA
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_audioOutput = new QAudioOutput(this);
    m_audioOutput->setVolume(1.0);
    m_player->setAudioOutput(m_audioOutput);
#else
    m_player->setVolume(100);
#endif
#else
    Q_UNUSED(parent);
#endif
}

PromptAudioService::~PromptAudioService()
{
    stopCurrentPlayback();
}

bool PromptAudioService::playPreset(const QString& presetId)
{
    const QString normalizedPresetId = normalizePresetId(presetId);
    if (normalizedPresetId.isEmpty()) {
        qWarning().noquote() << QStringLiteral("[PromptAudio] preset id is empty.");
        return false;
    }

#if !VTKQTCORE_HAS_QT_MULTIMEDIA
    qWarning().noquote() << QStringLiteral("[PromptAudio] Qt Multimedia is unavailable, preset playback is disabled.");
    return false;
#endif

    queueRequest({RequestKind::Preset, normalizedPresetId, QString()});
    return true;
}

bool PromptAudioService::playSource(const QString& source)
{
    const QString normalizedSource = normalizeSource(source);
    if (normalizedSource.isEmpty()) {
        qWarning().noquote() << QStringLiteral("[PromptAudio] source is empty.");
        return false;
    }

#if !VTKQTCORE_HAS_QT_MULTIMEDIA
    qWarning().noquote() << QStringLiteral("[PromptAudio] Qt Multimedia is unavailable, source playback is disabled.");
    return false;
#endif

    queueRequest({RequestKind::Source, QString(), normalizedSource});
    return true;
}

bool PromptAudioService::registerPreset(const QString& presetId, const QString& source)
{
    const QString normalizedPresetId = normalizePresetId(presetId);
    const QString normalizedSource = normalizeSource(source);
    if (normalizedPresetId.isEmpty() || normalizedSource.isEmpty()) {
        qWarning().noquote()
            << QStringLiteral("[PromptAudio] preset registration requires both presetId and source.");
        return false;
    }

    if (!QFile::exists(normalizedSource)) {
        qWarning().noquote()
            << QStringLiteral("[PromptAudio] source does not exist: %1").arg(normalizedSource);
        return false;
    }

    m_presetSources.insert(normalizedPresetId, normalizedSource);
#if VTKQTCORE_HAS_QT_MULTIMEDIA
    if (isWaveAudioSource(normalizedSource)) {
        ensureEffect(normalizedSource);
    }
#endif
    return true;
}

void PromptAudioService::stopPlayback()
{
    queueRequest({RequestKind::Stop, QString(), QString()});
}

void PromptAudioService::processPendingRequest()
{
    m_playQueued = false;

    const PlaybackRequest request = m_pendingRequest;
    m_pendingRequest = PlaybackRequest{};

    stopCurrentPlayback();
    if (request.kind == RequestKind::Stop || request.kind == RequestKind::None) {
        return;
    }

    startRequest(request);
}

void PromptAudioService::queueRequest(const PlaybackRequest& request)
{
    m_pendingRequest = request;
    if (m_playQueued) {
        return;
    }

    m_playQueued = true;
    QMetaObject::invokeMethod(this, &PromptAudioService::processPendingRequest,
                              Qt::QueuedConnection);
}

void PromptAudioService::stopCurrentPlayback()
{
#if VTKQTCORE_HAS_QT_MULTIMEDIA
    if (m_activeEffect) {
        m_activeEffect->stop();
        m_activeEffect.clear();
    }

    if (m_player) {
        m_player->stop();
    }
#endif
}

void PromptAudioService::startRequest(const PlaybackRequest& request)
{
    QString resolvedSource;
    if (request.kind == RequestKind::Preset) {
        resolvedSource = resolvePresetSource(request.presetId);
        if (resolvedSource.isEmpty()) {
            qWarning().noquote()
                << QStringLiteral("[PromptAudio] preset not registered: %1").arg(request.presetId);
            return;
        }
    } else {
        resolvedSource = request.source;
    }

    if (tryPlayWithSoundEffect(resolvedSource)) {
        return;
    }

    playWithMediaPlayer(resolvedSource);
}

bool PromptAudioService::tryPlayWithSoundEffect(const QString& source)
{
#if !VTKQTCORE_HAS_QT_MULTIMEDIA
    Q_UNUSED(source);
    return false;
#else
    if (!isWaveAudioSource(source)) {
        return false;
    }

    QSoundEffect* effect = ensureEffect(source);
    if (!effect) {
        return false;
    }

    m_activeEffect = effect;
    effect->stop();
    effect->play();
    return true;
#endif
}

void PromptAudioService::playWithMediaPlayer(const QString& source)
{
#if !VTKQTCORE_HAS_QT_MULTIMEDIA
    Q_UNUSED(source);
    qWarning().noquote() << QStringLiteral("[PromptAudio] Qt Multimedia is unavailable, media playback is disabled.");
    return;
#else
    const QUrl url = toUrl(source);
    if (!url.isValid()) {
        qWarning().noquote()
            << QStringLiteral("[PromptAudio] invalid audio url for source: %1").arg(source);
        return;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    m_player->setSource(url);
#else
    m_player->setMedia(url);
#endif
    m_player->play();
#endif
}

QSoundEffect* PromptAudioService::ensureEffect(const QString& source)
{
#if !VTKQTCORE_HAS_QT_MULTIMEDIA
    Q_UNUSED(source);
    return nullptr;
#else
    const QString normalizedSource = normalizeSource(source);
    auto it = m_effectCache.find(normalizedSource);
    if (it != m_effectCache.end()) {
        return it.value();
    }

    auto* effect = new QSoundEffect(this);
    effect->setLoopCount(1);
    effect->setVolume(1.0);
    effect->setSource(toUrl(normalizedSource));
    m_effectCache.insert(normalizedSource, effect);
    return effect;
#endif
}

QString PromptAudioService::resolvePresetSource(const QString& presetId) const
{
    return m_presetSources.value(normalizePresetId(presetId));
}

QString PromptAudioService::normalizePresetId(const QString& presetId) const
{
    QString normalized = presetId.trimmed().toLower();
    normalized.replace(QLatin1Char('-'), QLatin1Char('_'));
    return normalized;
}

bool PromptAudioService::isWaveAudioSource(const QString& source) const
{
    QFile file(normalizeSource(source));
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    const QByteArray header = file.read(12);
    return header.size() >= 12 &&
        header.startsWith("RIFF") &&
        header.mid(8, 4) == "WAVE";
}

QUrl PromptAudioService::toUrl(const QString& source) const
{
    const QString normalizedSource = normalizeSource(source);
    if (normalizedSource.startsWith(QStringLiteral(":/"))) {
        return QUrl(QStringLiteral("qrc%1").arg(normalizedSource));
    }

    return QUrl::fromLocalFile(normalizedSource);
}