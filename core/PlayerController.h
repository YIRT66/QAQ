#pragma once

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QStringList>

class QMediaPlayer;
class QAudioOutput;

class AudioWorker final : public QObject
{
    Q_OBJECT
public:
    explicit AudioWorker(QObject *parent = nullptr);

public slots:
    void initialize();
    void loadSource(const QString &url, quint64 generation);
    void clearSource(quint64 generation);
    void play(quint64 generation);
    void pause(quint64 generation);
    void stop(quint64 generation);
    void seek(qint64 positionMs, quint64 generation);
    void setVolume(double value);
    void setPlaybackRate(double value);
    void shutdown();

signals:
    void playbackStateUpdate(quint64 generation, bool playing);
    void positionUpdate(quint64 generation, qint64 positionMs);
    void durationUpdate(quint64 generation, qint64 durationMs);
    void sourceReady(quint64 generation);
    void endOfMedia(quint64 generation);
    void errorOccurred(quint64 generation, const QString &message);

private:
    bool accepts(quint64 generation) const { return generation == m_generation; }

    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audio = nullptr;
    QTimer *m_positionTimer = nullptr;
    qint64 m_pendingPosition = 0;
    qint64 m_publishedPosition = -1;
    quint64 m_generation = 0;
    double m_initialVolume = 0.55;
};

class PlayerController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap currentTrack READ currentTrack NOTIFY currentTrackChanged)
    Q_PROPERTY(QVariantList queue READ queue NOTIFY queueChanged)
    Q_PROPERTY(int currentIndex READ currentIndex NOTIFY currentIndexChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(QString engineStatus READ engineStatus NOTIFY engineStatusChanged)
    Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
    Q_PROPERTY(qint64 duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(double playbackRate READ playbackRate WRITE setPlaybackRate NOTIFY playbackRateChanged)
    Q_PROPERTY(int playMode READ playMode WRITE setPlayMode NOTIFY playModeChanged)
    Q_PROPERTY(QString sourceUrl READ sourceUrl NOTIFY sourceUrlChanged)
    Q_PROPERTY(bool webPlaybackActive READ webPlaybackActive NOTIFY webPlaybackActiveChanged)
public:
    explicit PlayerController(QObject *parent = nullptr);
    ~PlayerController() override;

    QVariantMap currentTrack() const { return m_currentTrack; }
    QVariantList queue() const { return m_queue; }
    int currentIndex() const { return m_currentIndex; }
    bool playing() const { return m_playing; }
    bool busy() const { return m_busy; }
    QString engineStatus() const { return m_engineStatus; }
    qint64 position() const { return m_position; }
    qint64 duration() const { return m_duration; }
    double volume() const { return m_volume; }
    double playbackRate() const { return m_playbackRate; }
    int playMode() const { return m_playMode; }
    QString sourceUrl() const { return m_sourceUrl; }
    bool webPlaybackActive() const { return m_webPlaybackActive; }

    void setResolvedSource(const QString &songId, const QString &url);
    void setResolvedSources(const QString &songId, const QStringList &urls);
    void appendResolvedSources(const QString &songId, const QStringList &urls);
    void beginWebPlayback(const QString &songId, const QString &pageUrl);
    void updateCurrentTrackMetadata(const QVariantMap &track);

    Q_INVOKABLE void setQueueAndPlay(const QVariantList &tracks, int index);
    Q_INVOKABLE void appendToQueue(const QVariantList &tracks);
    Q_INVOKABLE void playTrack(const QVariantMap &track);
    Q_INVOKABLE void togglePlay();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(qint64 positionMs);
    Q_INVOKABLE void setVolume(double value);
    Q_INVOKABLE void setPlaybackRate(double value);
    Q_INVOKABLE void setPlayMode(int mode);
    Q_INVOKABLE void cyclePlayMode();
    Q_INVOKABLE void clearQueue();
    Q_INVOKABLE void reportWebPlaybackState(bool ready, bool playing, qint64 positionMs, qint64 durationMs, bool ended);
    Q_INVOKABLE void reportWebPlaybackFailure(const QString &message);

signals:
    void currentTrackChanged();
    void queueChanged();
    void currentIndexChanged();
    void playingChanged();
    void busyChanged();
    void engineStatusChanged();
    void positionChanged();
    void durationChanged();
    void volumeChanged();
    void playbackRateChanged();
    void playModeChanged();
    void sourceUrlChanged();
    void webPlaybackActiveChanged();
    void webPlaybackRequested(const QString &url, double volume);
    void webPlaybackCommand(const QString &command, double value);
    void streamRequested(const QVariantMap &track);
    void playbackError(const QString &message);

private:
    void requestIndex(int index);
    int randomIndex() const;
    void setBusy(bool value, const QString &status = {});
    void invokeClear();
    bool retryNextSourceCandidate(const QString &reason = {});
    void loadCurrentSourceCandidate();

    QThread m_audioThread;
    AudioWorker *m_worker = nullptr;
    QVariantList m_queue;
    QVariantMap m_currentTrack;
    int m_currentIndex = -1;
    int m_playMode = 0; // 0 列表循环, 1 单曲循环, 2 随机
    QString m_sourceUrl;
    QStringList m_sourceCandidates;
    int m_sourceCandidateIndex = -1;
    bool m_playing = false;
    bool m_webPlaybackActive = false;
    bool m_busy = false;
    QString m_engineStatus;
    qint64 m_position = 0;
    qint64 m_duration = 0;
    double m_volume = 0.55;
    double m_playbackRate = 1.0;
    quint64 m_generation = 0;
    QTimer m_switchWatchdog;
};
