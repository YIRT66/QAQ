#pragma once

#include "IMusicProvider.h"

#include <QHash>
#include <QMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>
#include <functional>

class TwoT58Provider final : public IMusicProvider
{
    Q_OBJECT
public:
    explicit TwoT58Provider(QObject *parent = nullptr);

    QString providerId() const override { return QStringLiteral("2t58"); }
    QString displayName() const override { return QStringLiteral("爱听音乐 2t58"); }
    QString baseUrl() const override { return m_baseUrl; }
    void setBaseUrl(const QString &url) override;

    bool requiresSuccessfulPing() const override { return false; }
    bool supportsOnDemandResolve() const override { return true; }

    void search(const QString &keywords, int limit = 40) override;
    void loadHome(int limit = 14) override;
    void loadPlaylist(const QString &playlistId, int limit = 300) override;
    void resolveSongUrl(const QString &songId, const QString &level = "exhigh") override;
    void resolveDownloadUrl(const QString &songId, const QString &level = "exhigh") override;
    void loadLyrics(const QString &songId) override;
    void resolveTrack(const QVariantMap &track, const QString &level = "exhigh") override;
    void ping() override;

private:
    using TextCallback = std::function<void(const QString &, const QUrl &)>;
    using TracksCallback = std::function<void(const QVariantList &)>;

    struct CachedSearch {
        qint64 timestampMs = 0;
        QVariantList rows;
    };

    QString searchCookie() const;
    QMap<QString, QString> searchCompatHeaders() const;

    QNetworkReply *getText(const QUrl &url,
                           TextCallback success,
                           std::function<void(const QString &)> failure = {},
                           const QString &referer = QString(),
                           int timeoutMs = 2000);

    void fetchSearch(const QString &keywords,
                     int limit,
                     TracksCallback callback,
                     bool allowCache = true);

    QVariantList parseSearchPage(const QString &html,
                                 int limit,
                                 const QString &fallbackTitle = QString()) const;
    QVariantList parseIndexRows(const QVariantList &rows,
                                const QString &keywords,
                                int limit) const;

    QStringList songPageCandidates(const QString &sourceId) const;
    void resolvePageVariant(const QString &sourceId,
                            const QString &emitId,
                            int variant = 0);
    void resolveDirectMedia(const QString &sourceId,
                            const QString &emitId,
                            bool download,
                            int qualityIndex = 0);
    QStringList extractPlayableCandidates(const QString &html,
                                          const QUrl &pageUrl) const;
    QStringList extractResolverEndpoints(const QString &html,
                                         const QUrl &pageUrl) const;
    void probeCandidate(const QString &candidate,
                        const QString &referer,
                        const QString &emitId,
                        int depth = 0,
                        QStringList visited = {});
    void tryCandidateList(const QStringList &candidates,
                          const QString &referer,
                          const QString &emitId,
                          int index = 0);

    void loadLyricsFromPage(const QString &sourceId);
    QString extractLrcText(const QString &text) const;

    QString m_baseUrl = QStringLiteral("https://www.2t58.com");
    QNetworkAccessManager m_network;
    QHash<QString, CachedSearch> m_searchCache;
    quint64 m_searchSerial = 0;
};
