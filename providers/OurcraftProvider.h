#pragma once

#include "IMusicProvider.h"

#include <QJsonDocument>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPointer>
#include <QStringList>
#include <functional>

// Client for Yuncan050115/ourcraft-music-api.
// The backend exposes one MetingJS-compatible /api endpoint and supports
// netease / kugou / kuwo through the `server` query parameter.
class OurcraftProvider final : public IMusicProvider
{
    Q_OBJECT
public:
    OurcraftProvider(const QString &id,
                     const QString &name,
                     const QString &server,
                     const QString &defaultBaseUrl,
                     QObject *parent = nullptr);

    QString providerId() const override { return m_id; }
    QString displayName() const override { return m_name; }
    QString baseUrl() const override { return m_baseUrl; }
    void setBaseUrl(const QString &url) override;

    void search(const QString &keywords, int limit = 40) override;
    void loadHome(int limit = 14) override;
    void loadPlaylist(const QString &playlistId, int limit = 300) override;
    void resolveSongUrl(const QString &songId, const QString &level = "exhigh") override;
    void resolveDownloadUrl(const QString &songId, const QString &level = "exhigh") override;
    void loadLyrics(const QString &songId) override;
    void ping() override;

    // Search/playback can start immediately; ping is health information only.
    bool requiresSuccessfulPing() const override { return false; }

    // If the selected track only has one platform candidate, Ourcraft can
    // perform a title/artist lookup on the other enabled platforms and race a
    // fresh playable URL without involving Evolve Cloud.
    bool supportsOnDemandResolve() const override { return true; }
    void resolveTrack(const QVariantMap &track, const QString &level = "exhigh") override;
    void searchCatalog(const QString &keywords, int limit = 16);

signals:
    void discoveryTracksReady(const QVariantList &tracks);
    void catalogSearchReady(const QVariantList &playlists,
                            const QVariantList &artists);

private:
    QNetworkReply *get(const QList<QPair<QString, QString>> &query,
                       std::function<void(const QJsonDocument &, const QByteArray &)> success,
                       std::function<void(const QString &)> failure = {},
                       int timeoutMs = 7000);
    QVariantList extractRows(const QJsonDocument &doc) const;
    QNetworkReply *enrichNeteaseTracks(
        const QVariantList &tracks,
        std::function<void(const QVariantList &)> completion,
        int timeoutMs = 9000);
    QVariantMap parseTrack(const QJsonObject &track) const;
    QVariantMap parsePlaylistMeta(const QString &playlistId,
                                  const QVariantList &tracks) const;
    QString extractUrl(const QJsonDocument &doc, const QByteArray &raw) const;
    QString extractLyrics(const QJsonDocument &doc, const QByteArray &raw) const;
    static QString jsonString(const QJsonValue &value);
    static QString artistText(const QJsonValue &value);
    static qint64 durationMs(const QJsonValue &value);

    QString m_id;
    QString m_name;
    QString m_server;
    QString m_baseUrl;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_searchReply;
    QPointer<QNetworkReply> m_streamReply;
    QPointer<QNetworkReply> m_lookupReply;
    QPointer<QNetworkReply> m_lyricsReply;
    QPointer<QNetworkReply> m_playlistSearchReply;
    QPointer<QNetworkReply> m_artistSearchReply;
    QPointer<QNetworkReply> m_homePlaylistReply;
    quint64 m_searchGeneration = 0;
    quint64 m_catalogSearchGeneration = 0;
    quint64 m_homeGeneration = 0;
    quint64 m_playlistGeneration = 0;
};
