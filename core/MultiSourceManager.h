#pragma once

#include <QObject>
#include <QHash>
#include <QSet>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QStringList>

#include "../providers/IMusicProvider.h"

class MultiSourceManager : public QObject
{
    Q_OBJECT
public:
    explicit MultiSourceManager(QObject *parent = nullptr);

    void addProvider(IMusicProvider *provider, bool enabled, int priority);
    QVariantList providerStates() const;
    QString providerBaseUrl(const QString &providerId) const;
    bool providerEnabled(const QString &providerId) const;
    bool providerUsable(const QString &providerId) const;
    void setProviderEnabled(const QString &providerId, bool enabled);
    void setProviderBaseUrl(const QString &providerId, const QString &url);
    void setProviderPriority(const QString &providerId, int priority);

    void search(const QString &keywords, int limit = 40);
    void searchProviders(const QString &keywords, const QStringList &providerIds, int limit = 40);
    void loadHome(int limit = 14);
    void loadPlaylist(const QString &providerId, const QString &playlistId, int limit = 300);
    void resolveBestStream(const QVariantMap &track, const QString &quality,
                           const QStringList &excludedProviderIds = {});
    void resolveBestDownload(const QVariantMap &track, const QString &quality);
    void loadBestLyrics(const QVariantMap &track);
    void ping(const QString &providerId);
    void pingAll();

signals:
    void searchProgress(const QVariantList &tracks);
    void searchReady(const QVariantList &tracks);
    void homeReady(const QVariantList &playlists);
    void playlistReady(const QVariantMap &playlist, const QVariantList &tracks);
    void streamReady(const QString &canonicalTrackId, const QString &url,
                     const QString &providerId, const QString &providerName, const QString &access);
    void streamResolveFailed(const QString &canonicalTrackId, const QString &message);
    void downloadReady(const QString &canonicalTrackId, const QString &url,
                       const QString &providerId, const QString &providerName);
    void lyricsReady(const QString &canonicalTrackId, const QString &lyric, const QString &translatedLyric);
    void providerStateChanged();
    void pingReady(const QString &providerId, bool ok, const QString &message);
    void errorOccurred(const QString &message);

private:
    struct ProviderEntry {
        IMusicProvider *provider = nullptr;
        bool enabled = true;
        int priority = 100;
        // Built-in NetEase is assumed usable while the bundled local service is
        // starting. External gateways must pass ping before they enter hot paths.
        bool online = false;
        bool bundled = false;
        QString status = QStringLiteral("未检测");
    };

    struct ResolveState {
        QString canonicalId;
        QString quality;
        QVariantMap track;
        QVariantList candidates;
        int index = -1;
        QString activeProviderId;
        QString activeSourceId;
        QString lastError;
        bool download = false;

        // After all source IDs already attached to the canonical track have
        // failed, providers with supportsOnDemandResolve() can perform a fresh
        // title/artist lookup. This is crucial for public web sources whose
        // search HTML can change independently from the primary catalog.
        bool lookupPhase = false;
        QStringList excludedProviderIds;
        QStringList lookupProviderIds;
        int lookupIndex = -1;

        // v0.14.4 stream resolution races all usable providers instead of
        // waiting for each provider to fail sequentially. Downloads keep the
        // conservative sequential path below.
        bool parallelStream = false;
        QSet<QString> pendingProviderIds;
        QHash<QString, QString> expectedSourceIds;
        QHash<QString, QString> accessByProvider;
        QVariantMap provisionalResult;
    };

    static QString normalize(const QString &text);
    static QString canonicalIdFor(const QVariantMap &track);
    static int accessRank(const QString &access);
    QVariantMap annotateTrack(const QString &providerId, const QVariantMap &track) const;
    QVariantList mergeTracks(const QVariantList &existing, const QString &providerId, const QVariantList &incoming) const;
    QVariantList rankSearchResults(const QVariantList &rows) const;
    QVariantList sortedCandidates(const QVariantMap &track) const;
    IMusicProvider *provider(const QString &providerId) const;
    QString providerName(const QString &providerId) const;
    void startNextResolve();
    void startNextLookupResolve();
    void startNextLyricsCandidate();
    void startParallelStreamResolve();
    void finishParallelStreamResult(const QVariantMap &result);
    void maybeFinishParallelStream();

    QHash<QString, ProviderEntry> m_providers;
    QStringList m_providerOrder;

    QVariantList m_searchMerged;
    QString m_searchQuery;
    QSet<QString> m_searchWaiting;
    QTimer m_searchDeadline;
    QTimer m_streamPreferenceTimer;
    bool m_searchInitialReadyEmitted = false;
    QVariantList m_homeMerged;
    QSet<QString> m_homeWaiting;

    ResolveState m_resolve;
    QVariantMap m_lyricsTrack;
    QVariantList m_lyricsCandidates;
    int m_lyricsIndex = -1;
};
