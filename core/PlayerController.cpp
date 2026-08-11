#include "PlayerController.h"

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QMetaObject>
#include <QRandomGenerator>
#include <QSet>
#include <QUrl>

AudioWorker::AudioWorker(QObject *parent) : QObject(parent) {}

void AudioWorker::initialize()
{
    if (m_player) return;

    m_audio = new QAudioOutput(this);
    m_audio->setVolume(m_initialVolume);
    m_player = new QMediaPlayer(this);
    m_player->setAudioOutput(m_audio);

    // Position updates are throttled before they cross the thread boundary.
    // This keeps the GUI event queue bounded during hours of playback.
    m_positionTimer = new QTimer(this);
    m_positionTimer->setInterval(125);
    connect(m_positionTimer, &QTimer::timeout, this, [this] {
        if (m_publishedPosition == m_pendingPosition) return;
        m_publishedPosition = m_pendingPosition;
        emit positionUpdate(m_generation, m_publishedPosition);
    });
    m_positionTimer->start();

    connect(m_player, &QMediaPlayer::positionChanged, this, [this](qint64 value) {
        m_pendingPosition = value;
    });
    connect(m_player, &QMediaPlayer::durationChanged, this, [this](qint64 value) {
        emit durationUpdate(m_generation, value);
    });
    connect(m_player, &QMediaPlayer::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState state) {
        emit playbackStateUpdate(m_generation, state == QMediaPlayer::PlayingState);
    });
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::LoadedMedia || status == QMediaPlayer::BufferedMedia)
            emit sourceReady(m_generation);
        else if (status == QMediaPlayer::EndOfMedia)
            emit endOfMedia(m_generation);
    });
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error, const QString &text) {
        emit errorOccurred(m_generation,
                           text.isEmpty() ? QStringLiteral("播放器发生未知错误") : text);
    });
}

void AudioWorker::loadSource(const QString &url, quint64 generation)
{
    initialize();
    m_generation = generation;
    m_pendingPosition = 0;
    m_publishedPosition = -1;

    // Discard old network/decoder state first. The second step is intentionally
    // deferred to the worker event loop so backend teardown and setup are not
    // performed in one re-entrant call chain.
    m_player->stop();
    m_player->setSource(QUrl());

    QTimer::singleShot(25, this, [this, url, generation] {
        if (!m_player || !accepts(generation)) return;
        m_player->setSource(QUrl(url));
        m_player->play();
    });
}

void AudioWorker::clearSource(quint64 generation)
{
    initialize();
    m_generation = generation;
    m_pendingPosition = 0;
    m_publishedPosition = -1;
    m_player->stop();
    m_player->setSource(QUrl());
    emit positionUpdate(m_generation, 0);
    emit durationUpdate(m_generation, 0);
    emit playbackStateUpdate(m_generation, false);
}

void AudioWorker::play(quint64 generation)
{
    if (!m_player || !accepts(generation)) return;
    m_player->play();
}

void AudioWorker::pause(quint64 generation)
{
    if (!m_player || !accepts(generation)) return;
    m_player->pause();
}

void AudioWorker::stop(quint64 generation)
{
    if (!m_player || !accepts(generation)) return;
    m_player->stop();
}

void AudioWorker::seek(qint64 positionMs, quint64 generation)
{
    if (!m_player || !accepts(generation)) return;
    m_player->setPosition(qMax<qint64>(0, positionMs));
}

void AudioWorker::setVolume(double value)
{
    if (!m_audio) {
        m_initialVolume = qBound(0.0, value, 1.0);
        return;
    }
    m_audio->setVolume(qBound(0.0, value, 1.0));
}

void AudioWorker::setPlaybackRate(double value)
{
    initialize();
    if (m_player) m_player->setPlaybackRate(qBound(0.5, value, 2.0));
}

void AudioWorker::shutdown()
{
    if (m_positionTimer) m_positionTimer->stop();
    if (m_player) {
        m_player->stop();
        m_player->setSource(QUrl());
    }
}

PlayerController::PlayerController(QObject *parent) : QObject(parent)
{
    m_audioThread.setObjectName(QStringLiteral("EvolveMusicAudioThread"));
    m_worker = new AudioWorker;
    m_worker->moveToThread(&m_audioThread);
    connect(&m_audioThread, &QThread::started, m_worker, &AudioWorker::initialize);
    connect(&m_audioThread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &AudioWorker::positionUpdate, this,
            [this](quint64 generation, qint64 value) {
        if (generation != m_generation || m_position == value) return;
        m_position = value;
        emit positionChanged();
    }, Qt::QueuedConnection);
    connect(m_worker, &AudioWorker::durationUpdate, this,
            [this](quint64 generation, qint64 value) {
        if (generation != m_generation || m_duration == value) return;
        m_duration = value;
        emit durationChanged();
    }, Qt::QueuedConnection);
    connect(m_worker, &AudioWorker::playbackStateUpdate, this,
            [this](quint64 generation, bool value) {
        if (generation != m_generation || m_playing == value) return;
        m_playing = value;
        if (value) setBusy(false, QStringLiteral("正在播放"));
        emit playingChanged();
    }, Qt::QueuedConnection);
    connect(m_worker, &AudioWorker::sourceReady, this,
            [this](quint64 generation) {
        if (generation != m_generation) return;
        setBusy(false, QStringLiteral("音频已就绪"));
    }, Qt::QueuedConnection);
    connect(m_worker, &AudioWorker::endOfMedia, this,
            [this](quint64 generation) {
        if (generation != m_generation) return;
        if (m_playMode == 1) {
            seek(0);
            QMetaObject::invokeMethod(m_worker, [worker = m_worker, generation] {
                worker->play(generation);
            }, Qt::QueuedConnection);
        } else {
            next();
        }
    }, Qt::QueuedConnection);
    connect(m_worker, &AudioWorker::errorOccurred, this,
            [this](quint64 generation, const QString &text) {
        if (generation != m_generation) return;
        if (retryNextSourceCandidate(text)) return;
        setBusy(false, QStringLiteral("播放失败"));
        emit playbackError(text);
    }, Qt::QueuedConnection);

    m_switchWatchdog.setSingleShot(true);
    m_switchWatchdog.setInterval(7000);
    connect(&m_switchWatchdog, &QTimer::timeout, this, [this] {
        if (!m_busy) return;
        if (m_webPlaybackActive) {
            setBusy(false, QStringLiteral("网页播放源响应较慢"));
            emit playbackError(QStringLiteral("网页播放源加载超时"));
            return;
        }
        // A Worker range stream can pass the first probe yet stall later in the
        // platform media backend. Retry the alternate CDN URL automatically.
        if (retryNextSourceCandidate(QStringLiteral("音频准备超时"))) return;
        setBusy(false, QStringLiteral("音频引擎响应较慢"));
        emit playbackError(QStringLiteral("音频准备超时，所有云端候选地址均未能打开"));
    });

    m_audioThread.start();
}

PlayerController::~PlayerController()
{
    if (m_worker)
        QMetaObject::invokeMethod(m_worker, [worker = m_worker] { worker->shutdown(); },
                                  Qt::QueuedConnection);
    m_audioThread.quit();
    if (!m_audioThread.wait(1800)) {
        // Shutdown-only fallback: never let a wedged multimedia backend prevent
        // the process from closing forever.
        m_audioThread.requestInterruption();
        if (!m_audioThread.wait(400)) {
            m_audioThread.terminate();
            m_audioThread.wait();
        }
    }
}

void PlayerController::setBusy(bool value, const QString &status)
{
    if (m_busy != value) {
        m_busy = value;
        emit busyChanged();
    }
    if (!status.isEmpty() && m_engineStatus != status) {
        m_engineStatus = status;
        emit engineStatusChanged();
    }
    if (!value) m_switchWatchdog.stop();
}

void PlayerController::invokeClear()
{
    const quint64 generation = m_generation;
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, generation] {
        worker->clearSource(generation);
    }, Qt::QueuedConnection);
}

void PlayerController::loadCurrentSourceCandidate()
{
    if (m_sourceCandidateIndex < 0
        || m_sourceCandidateIndex >= m_sourceCandidates.size()) {
        return;
    }

    const QString url = m_sourceCandidates.at(m_sourceCandidateIndex);
    if (url.isEmpty()) return;

    if (m_sourceUrl != url) {
        m_sourceUrl = url;
        emit sourceUrlChanged();
    }

    setBusy(true,
            m_sourceCandidateIndex == 0
                ? QStringLiteral("正在打开音频…")
                : QStringLiteral("正在切换备用音频…"));
    // Once a concrete URL exists, fail over aggressively instead of waiting
    // 20 seconds on a dead CDN edge.
    m_switchWatchdog.setInterval(4200);
    m_switchWatchdog.start();

    const quint64 generation = m_generation;
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, url, generation] {
        worker->loadSource(url, generation);
    }, Qt::QueuedConnection);
}

bool PlayerController::retryNextSourceCandidate(const QString &reason)
{
    const int nextIndex = m_sourceCandidateIndex + 1;
    if (nextIndex < 0 || nextIndex >= m_sourceCandidates.size())
        return false;

    // Advance the internal generation so late decoder/network signals from the
    // failed candidate cannot cancel the replacement candidate.
    ++m_generation;
    m_sourceCandidateIndex = nextIndex;
    m_position = 0;
    m_duration = 0;
    m_playing = false;

    emit positionChanged();
    emit durationChanged();
    emit playingChanged();

    invokeClear();
    loadCurrentSourceCandidate();

    Q_UNUSED(reason);
    return true;
}

void PlayerController::beginWebPlayback(const QString &songId, const QString &pageUrl)
{
    if (m_currentTrack.value("id").toString() != songId || pageUrl.trimmed().isEmpty()) return;

    ++m_generation;
    invokeClear();
    m_sourceCandidates.clear();
    m_sourceCandidateIndex = -1;
    m_sourceUrl = pageUrl.trimmed();
    m_position = 0;
    m_duration = 0;
    m_playing = false;
    if (!m_webPlaybackActive) {
        m_webPlaybackActive = true;
        emit webPlaybackActiveChanged();
    }
    emit sourceUrlChanged();
    emit positionChanged();
    emit durationChanged();
    emit playingChanged();
    setBusy(true, QStringLiteral("正在打开网页播放源…"));
    m_switchWatchdog.setInterval(6200);
    m_switchWatchdog.start();
    emit webPlaybackRequested(m_sourceUrl, m_volume);
}

void PlayerController::reportWebPlaybackState(bool ready, bool playing, qint64 positionMs, qint64 durationMs, bool ended)
{
    if (!m_webPlaybackActive) return;
    if (ready) setBusy(false, QStringLiteral("网页播放源已就绪"));
    positionMs = qMax<qint64>(0, positionMs);
    durationMs = qMax<qint64>(0, durationMs);
    if (m_position != positionMs) { m_position = positionMs; emit positionChanged(); }
    if (m_duration != durationMs) { m_duration = durationMs; emit durationChanged(); }
    if (m_playing != playing) { m_playing = playing; emit playingChanged(); }
    if (ended) {
        if (m_playMode == 1) {
            emit webPlaybackCommand(QStringLiteral("seek"), 0.0);
            emit webPlaybackCommand(QStringLiteral("play"), 0.0);
        } else {
            next();
        }
    }
}

void PlayerController::reportWebPlaybackFailure(const QString &message)
{
    if (!m_webPlaybackActive) return;
    setBusy(false, QStringLiteral("网页播放源失败"));
    emit playbackError(message.isEmpty() ? QStringLiteral("网页播放源无法开始播放") : message);
}

void PlayerController::setResolvedSource(const QString &songId, const QString &url)
{
    setResolvedSources(songId, QStringList{url});
}

void PlayerController::setResolvedSources(const QString &songId, const QStringList &urls)
{
    if (m_currentTrack.value("id").toString() != songId) return;

    if (m_webPlaybackActive) {
        emit webPlaybackCommand(QStringLiteral("stop"), 0.0);
        m_webPlaybackActive = false;
        emit webPlaybackActiveChanged();
    }

    QStringList candidates;
    for (const QString &raw : urls) {
        const QString url = raw.trimmed();
        if (!url.isEmpty() && !candidates.contains(url))
            candidates.append(url);
    }

    if (candidates.isEmpty()) return;

    m_sourceCandidates = candidates;
    m_sourceCandidateIndex = 0;
    loadCurrentSourceCandidate();
}

void PlayerController::appendResolvedSources(const QString &songId, const QStringList &urls)
{
    if (m_currentTrack.value("id").toString() != songId)
        return;

    QStringList additions;
    for (const QString &raw : urls) {
        const QString url = raw.trimmed();
        if (!url.isEmpty()
            && !m_sourceCandidates.contains(url)
            && !additions.contains(url)) {
            additions.append(url);
        }
    }

    if (additions.isEmpty())
        return;

    // If no concrete audio source has been installed yet, this is equivalent
    // to the first resolver winning. Otherwise keep the late resolver output as
    // hot failover candidates without interrupting the song that is buffering.
    if (m_sourceCandidates.isEmpty() && !m_webPlaybackActive) {
        setResolvedSources(songId, additions);
        return;
    }

    if (!m_webPlaybackActive) {
        for (const QString &url : additions)
            m_sourceCandidates.append(url);
    }
}

void PlayerController::updateCurrentTrackMetadata(const QVariantMap &track)
{
    const QString currentId = m_currentTrack.value(QStringLiteral("id")).toString();
    if (currentId.isEmpty()
        || track.value(QStringLiteral("id")).toString() != currentId) {
        return;
    }

    QVariantMap merged = m_currentTrack;
    bool changed = false;
    for (const QString &key : {
             QStringLiteral("title"), QStringLiteral("artist"),
             QStringLiteral("album"), QStringLiteral("cover"),
             QStringLiteral("providerId"), QStringLiteral("providerName"),
             QStringLiteral("sourceId"), QStringLiteral("sourceSummary"),
             QStringLiteral("access"), QStringLiteral("audioType")}) {
        const QString value = track.value(key).toString().trimmed();
        if (!value.isEmpty() && merged.value(key).toString() != value) {
            merged[key] = track.value(key);
            changed = true;
        }
    }
    for (const QString &key : {
             QStringLiteral("duration"), QStringLiteral("bitrate"),
             QStringLiteral("sourceCount")}) {
        if (track.value(key).toLongLong() > 0
            && merged.value(key) != track.value(key)) {
            merged[key] = track.value(key);
            changed = true;
        }
    }
    if (!track.value(QStringLiteral("sources")).toList().isEmpty()
        && merged.value(QStringLiteral("sources"))
               != track.value(QStringLiteral("sources"))) {
        merged[QStringLiteral("sources")] = track.value(QStringLiteral("sources"));
        changed = true;
    }
    if (!changed) return;

    m_currentTrack = merged;
    if (m_currentIndex >= 0 && m_currentIndex < m_queue.size())
        m_queue[m_currentIndex] = merged;
    emit currentTrackChanged();
    emit queueChanged();
}

void PlayerController::setQueueAndPlay(const QVariantList &tracks, int index)
{
    m_queue = tracks;
    emit queueChanged();
    requestIndex(index);
}

void PlayerController::appendToQueue(const QVariantList &tracks)
{
    if (tracks.isEmpty())
        return;

    QSet<QString> existing;
    for (const QVariant &value : m_queue) {
        const QString id = value.toMap().value(QStringLiteral("id")).toString();
        if (!id.isEmpty())
            existing.insert(id);
    }

    bool changed = false;
    for (const QVariant &value : tracks) {
        const QVariantMap track = value.toMap();
        const QString id = track.value(QStringLiteral("id")).toString();
        if (track.isEmpty() || (!id.isEmpty() && existing.contains(id)))
            continue;
        m_queue.append(track);
        if (!id.isEmpty())
            existing.insert(id);
        changed = true;
    }

    if (changed)
        emit queueChanged();
}

void PlayerController::playTrack(const QVariantMap &track)
{
    const QString id = track.value("id").toString();
    for (int i = 0; i < m_queue.size(); ++i) {
        if (m_queue.at(i).toMap().value("id").toString() == id) {
            requestIndex(i);
            return;
        }
    }
    m_queue = {track};
    emit queueChanged();
    requestIndex(0);
}

void PlayerController::requestIndex(int index)
{
    if (m_queue.isEmpty()) return;
    index = qBound(0, index, static_cast<int>(m_queue.size()) - 1);
    ++m_generation;
    if (m_webPlaybackActive) {
        emit webPlaybackCommand(QStringLiteral("stop"), 0.0);
        m_webPlaybackActive = false;
        emit webPlaybackActiveChanged();
    }

    const bool indexChanged = m_currentIndex != index;
    m_currentIndex = index;
    m_currentTrack = m_queue.at(index).toMap();
    m_sourceUrl.clear();
    m_sourceCandidates.clear();
    m_sourceCandidateIndex = -1;
    m_position = 0;
    m_duration = 0;
    m_playing = false;
    setBusy(true, QStringLiteral("正在选择音源…"));
    m_switchWatchdog.setInterval(7000);
    m_switchWatchdog.start();
    invokeClear();

    if (indexChanged) emit currentIndexChanged();
    emit currentTrackChanged();
    emit sourceUrlChanged();
    emit positionChanged();
    emit durationChanged();
    emit playingChanged();
    emit streamRequested(m_currentTrack);
}

void PlayerController::clearQueue()
{
    ++m_generation;
    if (m_webPlaybackActive) {
        emit webPlaybackCommand(QStringLiteral("stop"), 0.0);
        m_webPlaybackActive = false;
        emit webPlaybackActiveChanged();
    }

    m_switchWatchdog.stop();
    invokeClear();

    m_queue.clear();
    m_currentTrack.clear();
    m_currentIndex = -1;
    m_sourceUrl.clear();
    m_sourceCandidates.clear();
    m_sourceCandidateIndex = -1;
    m_position = 0;
    m_duration = 0;
    m_playing = false;

    setBusy(false, QStringLiteral("已切换本地用户"));

    emit queueChanged();
    emit currentTrackChanged();
    emit currentIndexChanged();
    emit sourceUrlChanged();
    emit positionChanged();
    emit durationChanged();
    emit playingChanged();
}

void PlayerController::togglePlay()
{
    if (m_currentTrack.isEmpty()) return;
    if (m_webPlaybackActive) {
        emit webPlaybackCommand(QStringLiteral("toggle"), 0.0);
        return;
    }
    if (m_sourceUrl.isEmpty()) {
        setBusy(true, QStringLiteral("正在重新解析音源…"));
        m_switchWatchdog.setInterval(7000);
        m_switchWatchdog.start();
        emit streamRequested(m_currentTrack);
        return;
    }
    const quint64 generation = m_generation;
    if (m_playing) {
        QMetaObject::invokeMethod(m_worker, [worker = m_worker, generation] {
            worker->pause(generation);
        }, Qt::QueuedConnection);
    } else {
        QMetaObject::invokeMethod(m_worker, [worker = m_worker, generation] {
            worker->play(generation);
        }, Qt::QueuedConnection);
    }
}

int PlayerController::randomIndex() const
{
    if (m_queue.size() <= 1) return 0;
    int nextIndex = m_currentIndex;
    while (nextIndex == m_currentIndex)
        nextIndex = QRandomGenerator::global()->bounded(static_cast<int>(m_queue.size()));
    return nextIndex;
}

void PlayerController::next()
{
    if (m_queue.isEmpty()) return;
    if (m_playMode == 2) requestIndex(randomIndex());
    else requestIndex((m_currentIndex + 1) % static_cast<int>(m_queue.size()));
}

void PlayerController::previous()
{
    if (m_queue.isEmpty()) return;
    if (m_position > 5000) {
        seek(0);
        return;
    }
    if (m_playMode == 2) requestIndex(randomIndex());
    else requestIndex((m_currentIndex - 1 + static_cast<int>(m_queue.size())) % static_cast<int>(m_queue.size()));
}

void PlayerController::seek(qint64 positionMs)
{
    if (m_webPlaybackActive) {
        const qint64 maxPosition = m_duration > 0 ? m_duration : qMax<qint64>(0, positionMs);
        const qint64 target = qBound<qint64>(0, positionMs, maxPosition);
        if (m_position != target) { m_position = target; emit positionChanged(); }
        emit webPlaybackCommand(QStringLiteral("seek"), static_cast<double>(target));
        return;
    }
    const qint64 maxPosition = m_duration > 0 ? m_duration : qMax<qint64>(0, positionMs);
    const qint64 target = qBound<qint64>(0, positionMs, maxPosition);
    if (m_position != target) {
        m_position = target;
        emit positionChanged();
    }
    const quint64 generation = m_generation;
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, target, generation] {
        worker->seek(target, generation);
    }, Qt::QueuedConnection);
}

void PlayerController::setVolume(double value)
{
    value = qBound(0.0, value, 1.0);
    if (qFuzzyCompare(m_volume, value)) return;
    m_volume = value;
    emit volumeChanged();
    if (m_webPlaybackActive) emit webPlaybackCommand(QStringLiteral("volume"), value);
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, value] {
        worker->setVolume(value);
    }, Qt::QueuedConnection);
}

void PlayerController::setPlaybackRate(double value)
{
    value = qBound(0.5, value, 2.0);
    if (qFuzzyCompare(m_playbackRate, value)) return;
    m_playbackRate = value;
    emit playbackRateChanged();
    if (m_webPlaybackActive)
        emit webPlaybackCommand(QStringLiteral("rate"), value);
    QMetaObject::invokeMethod(m_worker, [worker = m_worker, value] {
        worker->setPlaybackRate(value);
    }, Qt::QueuedConnection);
}

void PlayerController::setPlayMode(int mode)
{
    mode = qBound(0, mode, 2);
    if (mode == m_playMode) return;
    m_playMode = mode;
    emit playModeChanged();
}

void PlayerController::cyclePlayMode()
{
    setPlayMode((m_playMode + 1) % 3);
}
